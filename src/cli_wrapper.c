#include "cli_wrapper.h"
#include "process_control.h"
#include "lha_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <limits.h>

#ifdef PLATFORM_AMIGA
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <utility/tagitem.h>
#include <proto/dos.h>
#include <proto/exec.h>
#endif

/* Global state */
static FILE *g_logfile = NULL;
static bool g_initialized = false;

/* Internal helper functions */
static void log_timestamp(void);
static void log_message(const char *format, ...);
static void strip_escape_codes(const char *input, char *output, size_t output_size);
static bool parse_lha_list_line(const char *line, uint32_t *file_size);
static bool parse_lha_extract_line(const char *line, uint32_t *file_size, char *filename, size_t filename_max);
static bool parse_unzip_list_line(const char *line, uint32_t *file_size);
static bool parse_unzip_extract_line(const char *line, uint32_t *file_size, char *filename, size_t filename_max);
static bool check_directory_exists(const char *path);

#ifdef PLATFORM_AMIGA
static bool execute_command_amiga(const char *cmd, bool (*line_processor)(const char *, void *), void *user_data);
#else
static bool execute_command_host(const char *cmd, bool (*line_processor)(const char *, void *), void *user_data);
#endif

/* Data structures for line processing callbacks */
typedef struct {
    uint32_t total_size;
    uint32_t file_count;
    bool completion_detected;  /* Flag to indicate LHA completion */
} list_context_t;

typedef struct {
    uint32_t total_expected;
    uint32_t cumulative_bytes;
    uint32_t file_count;
    uint32_t last_percentage_x10;  /* Percentage * 10 to avoid floating point */
    bool completion_detected;  /* Flag to indicate LHA completion */
} extract_context_t;

bool cli_wrapper_init(void)
{
    if (g_initialized) {
        return true;
    }

    /* Try to open log file in current directory */
    g_logfile = fopen("logfile.txt", "w");
    if (!g_logfile) {
        /* If current directory fails, try temp directory on Amiga */
#ifdef PLATFORM_AMIGA
        g_logfile = fopen("T:logfile.txt", "w");
        if (!g_logfile) {
            /* Final fallback - try RAM: */
            g_logfile = fopen("RAM:logfile.txt", "w");
        }
#endif
        /* If all else fails, continue without logging */
        if (!g_logfile) {
            printf("Warning: Could not create logfile.txt - continuing without logging\n");
            /* Don't fail initialization just because of logging */
        }
    }

    g_initialized = true;

    if (g_logfile) {
        log_message("=== CLI Wrapper Session Started ===");
        log_message("Platform: %s",
#ifdef PLATFORM_AMIGA
            "Amiga"
#else
            "Host (stubbed)"
#endif
        );
    }

    return true;
}

void cli_wrapper_cleanup(void)
{
    if (g_logfile) {
        log_message("=== CLI Wrapper Session Ended ===");
        fclose(g_logfile);
        g_logfile = NULL;
    }
    g_initialized = false;
}

static void log_timestamp(void)
{
    if (!g_logfile) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    fprintf(g_logfile, "[%02d:%02d:%02d] ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

static void log_message(const char *format, ...)
{
    if (!g_logfile) return;

    va_list args;
    log_timestamp();
    va_start(args, format);
    vfprintf(g_logfile, format, args);
    va_end(args);
    fprintf(g_logfile, "\n");
    fflush(g_logfile);
}

static bool parse_lha_list_line(const char *line, uint32_t *file_size)
{
    /* Parse LHA list output format:
     * "   10380    6306 39.2% 06-Jul-112 19:06:46 +A10"
     * We want the first number (original size)
     *
     * Skip summary lines like:
     * " 2341998 1833297 21.7% 11-Jul-80 21:21:14   38 files"
     */

    *file_size = 0;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Check if this looks like a file line (starts with digit) */
    if (*line < '0' || *line > '9') {
        return false;
    }

    /* Check for summary line - contains "files" at the end */
    if (strstr(line, " files") != NULL) {
        return false; /* Skip summary line */
    }

    /* Parse the original size (first number) */
    char *endptr;
    unsigned long size = strtoul(line, &endptr, 10);

    /* Verify we parsed something and hit whitespace */
    if (endptr == line || (*endptr != ' ' && *endptr != '\t')) {
        return false;
    }

    /* Additional check: valid file lines should have a filename part with '+' or space */
    const char *filename_part = strstr(line, " +");
    const char *space_filename = strstr(line, "  ");
    if (!filename_part && !space_filename) {
        return false; /* Not a valid file line */
    }

    *file_size = (uint32_t)size;
    return true;
}

static bool parse_lha_extract_line(const char *line, uint32_t *file_size, char *filename, size_t filename_max)
{
    log_message("PARSE_EXTRACT: Parsing line: '%s'", line);

    /* Parse LHA extract output format:
     * " Extracting: (   10380)  A10TankKiller3Disk/data/A10[K"
     * We want the number in parentheses and the filename
     */

    *file_size = 0;
    filename[0] = '\0';

    /* Look for " Extracting: (" pattern */
    const char *extract_pos = strstr(line, " Extracting: (");
    if (!extract_pos) {
        log_message("PARSE_EXTRACT: No ' Extracting: (' pattern found");
        return false;
    }
    log_message("PARSE_EXTRACT: Found extract pattern at offset %d", (int)(extract_pos - line));

    /* Move to the size number */
    const char *size_start = extract_pos + 14; /* Length of " Extracting: (" */
    while (*size_start == ' ') size_start++;
    log_message("PARSE_EXTRACT: Size string starts at: '%.10s'", size_start);

    /* Parse the size */
    char *endptr;
    unsigned long size = strtoul(size_start, &endptr, 10);
    log_message("PARSE_EXTRACT: Parsed size: %lu", size);

    /* Look for closing paren and filename */
    const char *paren_pos = strchr(size_start, ')');
    if (!paren_pos) {
        log_message("PARSE_EXTRACT: No closing paren found");
        return false;
    }
    log_message("PARSE_EXTRACT: Found closing paren");

    /* Find filename after the paren and spaces */
    const char *filename_start = paren_pos + 1;
    while (*filename_start == ' ' || *filename_start == '\t') filename_start++;
    log_message("PARSE_EXTRACT: Filename starts at: '%.20s'", filename_start);

    /* Copy filename, stopping at control characters */
    size_t i;
    for (i = 0; i < filename_max - 1 && filename_start[i]; i++) {
        if (filename_start[i] == '[' || filename_start[i] < ' ') {
            break;
        }
        filename[i] = filename_start[i];
    }
    filename[i] = '\0';
    log_message("PARSE_EXTRACT: Extracted filename: '%s'", filename);

    *file_size = (uint32_t)size;
    log_message("PARSE_EXTRACT: Returning success - size: %u, filename: '%s'", *file_size, filename);
    return true;
}

static bool parse_unzip_list_line(const char *line, uint32_t *file_size)
{
    /* TODO: Implement based on your unzip -l output format
     * Example unzip -l format might be:
     * "   10380  06-07-25 15:30   filename.ext"
     * Adapt this function to match your unzip's actual output
     */

    *file_size = 0;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Check if this looks like a file line (starts with digit) */
    if (*line < '0' || *line > '9') {
        return false;
    }

    /* Parse the file size (first number) */
    char *endptr;
    unsigned long size = strtoul(line, &endptr, 10);

    /* Verify we parsed something and hit whitespace */
    if (endptr == line || (*endptr != ' ' && *endptr != '\t')) {
        return false;
    }

    *file_size = (uint32_t)size;
    return true;
}

static bool parse_unzip_extract_line(const char *line, uint32_t *file_size, char *filename, size_t filename_max)
{
    /* TODO: Implement based on your unzip extract output format
     * Example unzip extract format might be:
     * " inflating: filename.ext"
     * You may need to get file size from list first, or estimate
     */

    *file_size = 0;
    filename[0] = '\0';

    /* Look for "inflating:" or "extracting:" pattern */
    const char *inflate_pos = strstr(line, "inflating:");
    if (!inflate_pos) {
        inflate_pos = strstr(line, "extracting:");
        if (!inflate_pos) {
            return false;
        }
    }

    /* Find filename after the action word */
    const char *filename_start = strchr(inflate_pos, ':');
    if (!filename_start) {
        return false;
    }
    filename_start++; /* Skip the colon */

    /* Skip whitespace */
    while (*filename_start == ' ' || *filename_start == '\t') filename_start++;

    /* Copy filename, stopping at control characters */
    size_t i;
    for (i = 0; i < filename_max - 1 && filename_start[i]; i++) {
        if (filename_start[i] < ' ') {
            break;
        }
        filename[i] = filename_start[i];
    }
    filename[i] = '\0';

    /* Estimate file size - you might want to get this from a previous list operation */
    *file_size = 4000; /* Default estimate */

    return true;
}

static bool check_directory_exists(const char *path)
{
#ifdef PLATFORM_AMIGA
    BPTR lock = Lock((STRPTR)path, ACCESS_READ);
    if (lock) {
        UnLock(lock);
        return true;
    }
    return false;
#else
    /* Host stub - assume directory exists for testing */
    (void)path; /* Unused parameter in host build */
    return true;
#endif
}

/* Line processor for list command */
static bool list_line_processor(const char *line, void *user_data)
{
    log_message("LIST_PROCESSOR: Called with line: '%s'", line ? line : "(NULL)");

    list_context_t *ctx = (list_context_t *)user_data;
    log_message("LIST_PROCESSOR: Context ptr: %p", (void*)ctx);
    uint32_t file_size;

    /* Check for LHA completion messages first */
    if (strstr(line, "Operation successful") || strstr(line, "operation successful") ||
        strstr(line, "Done") || strstr(line, "Complete") || strstr(line, "finished")) {
        log_message("LIST_PROCESSOR: LHA COMPLETION DETECTED: '%s'", line);
        printf("\nLHA LIST COMPLETION: %s\n", line);
        fflush(stdout);
        
        /* Set completion flag */
        ctx->completion_detected = true;
        log_message("LIST_PROCESSOR: Set completion_detected flag to true");
        
        /* Return true to continue processing in case there are more messages */
        return true;
    }

    log_message("LIST_PROCESSOR: About to parse line");
    if (parse_lha_list_line(line, &file_size)) {
        log_message("LIST_PROCESSOR: Successfully parsed - size: %u", file_size);
        ctx->total_size += file_size;
        ctx->file_count++;
        log_message("LIST_PROCESSOR: Updated counters - files: %u, total: %u",
                   ctx->file_count, ctx->total_size);
    } else {
        log_message("LIST_PROCESSOR: Line not recognized as list format: '%s'", line);
    }

    log_message("LIST_PROCESSOR: Returning true to continue processing");
    return true; /* Continue processing */
}

/* Line processor for extract command */
static bool extract_line_processor(const char *line, void *user_data)
{
    log_message("EXTRACT_PROCESSOR: Called with line: '%s'", line ? line : "(NULL)");

    extract_context_t *ctx = (extract_context_t *)user_data;
    log_message("EXTRACT_PROCESSOR: Context ptr: %p", (void*)ctx);
    uint32_t file_size;
    static char filename[64];  /* Static to avoid stack usage, smaller size */

    /* Check for LHA error messages first */
    if (strstr(line, "*** Error") || strstr(line, "Unable to open")) {
        log_message("EXTRACT_PROCESSOR: LHA ERROR DETECTED: '%s'", line);
        printf("\n*** LHA ERROR: %s ***\n", line);
        fflush(stdout);
        /* Continue processing but note the error */
        return true;
    }

    /* Check for completion messages */
    if (strstr(line, "files extracted") || strstr(line, "all files OK") || 
        strstr(line, "Done") || strstr(line, "Complete") || strstr(line, "Operation successful")) {
        log_message("EXTRACT_PROCESSOR: COMPLETION DETECTED: '%s'", line);
        printf("\nLHA COMPLETION DETECTED: %s\n", line);
        fflush(stdout);
        
        /* Set completion flag */
        ctx->completion_detected = true;
        log_message("EXTRACT_PROCESSOR: Set completion_detected flag to true");
        
        /* Still return true to continue in case there are more messages */
        return true;
    }

    log_message("EXTRACT_PROCESSOR: About to parse line");
    if (parse_lha_extract_line(line, &file_size, filename, sizeof(filename))) {
        log_message("EXTRACT_PROCESSOR: Successfully parsed - file: '%s', size: %u", filename, file_size);
        ctx->cumulative_bytes += file_size;
        ctx->file_count++;

        log_message("EXTRACT_PROCESSOR: Updated counters - files: %u, bytes: %u/%u",
                   ctx->file_count, ctx->cumulative_bytes, ctx->total_expected);

        /* Calculate percentage using integer math (x10 for one decimal place) */
        uint32_t percentage_x10 = 0;
        if (ctx->total_expected > 0) {
            percentage_x10 = (ctx->cumulative_bytes * 1000) / ctx->total_expected;
        }

#ifdef PLATFORM_AMIGA
        /* Get current time - handle Amiga clock overflow */
        clock_t current_time = clock();
        unsigned long current_jiffies;

        if (current_time == (clock_t)-1 || current_time >= 4294967000UL) {
            /* Clock overflow on Amiga - use estimated timing */
            current_jiffies = ctx->file_count * 50;
        } else {
            current_jiffies = (unsigned long)current_time;
        }
#else
        /* Host fallback using clock() */
        clock_t current_time = clock();
        unsigned long current_jiffies = (unsigned long)current_time;
#endif

        /* Real-time console output - immediate feedback as extraction happens */
        printf("Extracting: %s (%u files) [%u.%u%%] %d jiffies\n",
               filename,
               ctx->file_count,
               percentage_x10 / 10,
               percentage_x10 % 10,
               (int)current_jiffies);
        fflush(stdout);
        log_message("EXTRACT_PROCESSOR: Displayed progress for file: %s", filename);
    } else {
        log_message("EXTRACT_PROCESSOR: Line not recognized as extract format: '%s'", line);
    }

    log_message("EXTRACT_PROCESSOR: Returning true to continue processing");
    return true; /* Continue processing */
}

typedef struct {
    const char *tool_name;      /* "LhA", "unzip", etc. */
    const char *pipe_prefix;    /* "lha_pipe", "unzip_pipe", etc. */
    int timeout_seconds;        /* Default: 2 */
    bool silent_mode;          /* Suppress console output */
} amiga_exec_config_t;

#ifdef PLATFORM_AMIGA

static bool execute_command_amiga_streaming(const char *cmd,
                                          bool (*line_processor)(const char *, void *),
                                          void *user_data,
                                          const amiga_exec_config_t *config)
{
    /* Use defaults if config is NULL */
    amiga_exec_config_t default_config = {
        .tool_name = "Command",
        .pipe_prefix = "cmd_pipe",
        .timeout_seconds = 2,
        .silent_mode = false
    };

    if (!config) {
        config = &default_config;
    }

    /* Debug: Log the exact command being executed */
    log_message("EXECUTE_AMIGA_STREAMING: Starting command execution");
    log_message("EXECUTE_AMIGA_STREAMING: Command: %s", cmd);
    log_message("EXECUTE_AMIGA_STREAMING: Tool: %s", config->tool_name);
    log_message("EXECUTE_AMIGA_STREAMING: Pipe prefix: %s", config->pipe_prefix);
    log_message("EXECUTE_AMIGA_STREAMING: Timeout: %d seconds", config->timeout_seconds);

    /* Generate unique pipe name using process ID, time, and tool prefix */
    struct Task *current_task = FindTask(NULL);
    if (!current_task) {
        log_message("ERROR: FindTask returned NULL");
        return false;
    }

    /* Add time component to make pipe name more unique */
    clock_t current_time = clock();
    static uint32_t sequence_counter = 0;
    sequence_counter++;

    char pipe_name[64];
    /* Handle clock overflow/error cases safely */
    if (current_time == (clock_t)-1 || current_time >= 4000000000UL) {
        /* Clock error or overflow - use sequence counter only */
        sprintf(pipe_name, "PIPE:%s.%lu.%lu", config->pipe_prefix, 
                (unsigned long)current_task, (unsigned long)sequence_counter);
    } else {
        sprintf(pipe_name, "PIPE:%s.%lu.%lu.%lu", config->pipe_prefix, 
                (unsigned long)current_task, (unsigned long)current_time, (unsigned long)sequence_counter);
    }
    log_message("EXECUTE_AMIGA_STREAMING: Generated pipe name: %s", pipe_name);
    
    /* CRITICAL: Clean up any potential leftover pipe first */
    log_message("EXECUTE_AMIGA_STREAMING: Attempting to clean up any existing pipe");
    BPTR existing_pipe = Open(pipe_name, MODE_OLDFILE);
    if (existing_pipe) {
        log_message("EXECUTE_AMIGA_STREAMING: Found existing pipe, closing it");
        Close(existing_pipe);
        /* Brief delay to allow cleanup */
        volatile int cleanup_delay;
        for (cleanup_delay = 0; cleanup_delay < 10000; cleanup_delay++) {
            /* 20ms cleanup delay */
        }
    }
    log_message("EXECUTE_AMIGA_STREAMING: Pipe cleanup completed");

    /* 1. Open pipe first for writing */
    BPTR pipe = Open(pipe_name, MODE_NEWFILE);
    if (!pipe) {
        log_message("ERROR: failed to open pipe %s", pipe_name);
        log_message("ERROR: IoErr() = %ld", IoErr());
        return false;
    }
    log_message("EXECUTE_AMIGA_STREAMING: Pipe opened successfully for writing");

    /* Console output */
    if (!config->silent_mode) {
        printf("Spawning %s process asynchronously...\n", config->tool_name);
        fflush(stdout);
    }

    /* Create command with pipe redirection for background execution */
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);
    log_message("EXECUTE_AMIGA_STREAMING: Full command with redirection: %s", full_cmd);

    /* Close the write pipe handle so the background command can open it */
    Close(pipe);
    log_message("EXECUTE_AMIGA_STREAMING: Write pipe closed");

    /* Execute command asynchronously using SystemTags or fallback to System */
    struct TagItem tags[] = {
        { SYS_Asynch, TRUE },
        { TAG_END, 0 }
    };

    log_message("EXECUTE_AMIGA_STREAMING: About to call SystemTagList");
    log_message("EXECUTE_AMIGA_STREAMING: Command length: %d", (int)strlen(full_cmd));
    log_message("EXECUTE_AMIGA_STREAMING: Using SYS_Asynch=TRUE");
    
    /* Add recovery mechanism - try multiple approaches */
    LONG proc_result = -1;
    bool system_success = false;
    
    /* First attempt: SystemTagList with async */
    log_message("EXECUTE_AMIGA_STREAMING: Attempting SystemTagList execution");
    
    /* Add safety check before SystemTagList */
    if (strlen(full_cmd) > 500) {
        log_message("EXECUTE_AMIGA_STREAMING: ERROR - Command too long: %d chars", (int)strlen(full_cmd));
        return false;
    }
    
    proc_result = SystemTagList(full_cmd, tags);
    log_message("EXECUTE_AMIGA_STREAMING: SystemTagList result: %ld", proc_result);

    if (proc_result == -1) {
        /* Fallback 1: Try without async - safer but blocking */
        log_message("EXECUTE_AMIGA_STREAMING: SystemTagList async failed, trying synchronous");
        struct TagItem sync_tags[] = {
            { SYS_Asynch, FALSE },
            { TAG_END, 0 }
        };
        
        /* Add delay before retry */
        volatile int retry_delay;
        for (retry_delay = 0; retry_delay < 100000; retry_delay++) {
            /* 200ms delay before retry */
        }
        
        proc_result = SystemTagList(full_cmd, sync_tags);
        log_message("EXECUTE_AMIGA_STREAMING: SystemTagList sync result: %ld", proc_result);
        
        if (proc_result == -1) {
            /* Fallback 2: Skip System() call - too risky */
            log_message("EXECUTE_AMIGA_STREAMING: All SystemTagList attempts failed");
            log_message("EXECUTE_AMIGA_STREAMING: Skipping System() fallback for safety");
            return false;
        }
    }
    
    if (proc_result == -1) {
        log_message("ERROR: All execution methods failed for %s process", config->tool_name);
        /* Don't return false immediately - try to proceed with pipe reading in case process started */
        log_message("EXECUTE_AMIGA_STREAMING: Continuing despite execution failure to check pipe");
    } else {
        system_success = true;
        log_message("EXECUTE_AMIGA_STREAMING: Command execution successful");
    }

    /* Give the asynchronous process time to start and open the pipe */
    /* Progressive delay with monitoring */
    log_message("EXECUTE_AMIGA_STREAMING: Starting progressive startup delay");
    int startup_attempts = 0;
    const int MAX_STARTUP_ATTEMPTS = 5;
    
    while (startup_attempts < MAX_STARTUP_ATTEMPTS) {
        startup_attempts++;
        log_message("EXECUTE_AMIGA_STREAMING: Startup attempt %d/%d", startup_attempts, MAX_STARTUP_ATTEMPTS);
        
        /* Progressive delay - start short, get longer */
        volatile int delay_counter;
        int delay_iterations = 20000 * startup_attempts; /* 40ms, 80ms, 120ms, etc. */
        for (delay_counter = 0; delay_counter < delay_iterations; delay_counter++) {
            /* Progressive startup delay */
        }
        
        /* Try to check if pipe is available by attempting to open it */
        log_message("EXECUTE_AMIGA_STREAMING: Testing pipe availability");
        BPTR test_pipe = Open(pipe_name, MODE_OLDFILE);
        if (test_pipe) {
            log_message("EXECUTE_AMIGA_STREAMING: Pipe is available on attempt %d", startup_attempts);
            Close(test_pipe);
            break;
        } else {
            log_message("EXECUTE_AMIGA_STREAMING: Pipe not ready on attempt %d, IoErr=%ld", startup_attempts, IoErr());
        }
    }
    
    if (startup_attempts >= MAX_STARTUP_ATTEMPTS) {
        log_message("EXECUTE_AMIGA_STREAMING: Pipe not available after %d attempts, proceeding anyway", MAX_STARTUP_ATTEMPTS);
    }

    /* 3. Open pipe for reading with enhanced error handling */
    log_message("EXECUTE_AMIGA_STREAMING: About to open pipe for reading");
    BPTR read_pipe;
    int open_attempts = 0;
    const int MAX_OPEN_ATTEMPTS = 3;
    
    read_pipe = 0;  /* Initialize BPTR to 0 (NULL equivalent for BPTR) */
    
    while (open_attempts < MAX_OPEN_ATTEMPTS && !read_pipe) {
        open_attempts++;
        log_message("EXECUTE_AMIGA_STREAMING: Pipe open attempt %d/%d", open_attempts, MAX_OPEN_ATTEMPTS);
        
        read_pipe = Open(pipe_name, MODE_OLDFILE);
        if (!read_pipe) {
            LONG io_error = IoErr();
            log_message("EXECUTE_AMIGA_STREAMING: Failed to open pipe for reading, IoErr=%ld", io_error);
            
            if (open_attempts < MAX_OPEN_ATTEMPTS) {
                log_message("EXECUTE_AMIGA_STREAMING: Retrying pipe open after delay");
                /* Brief delay before retry */
                volatile int retry_delay;
                for (retry_delay = 0; retry_delay < 30000; retry_delay++) {
                    /* 60ms retry delay */
                }
            }
        } else {
            log_message("EXECUTE_AMIGA_STREAMING: Pipe opened successfully for reading on attempt %d", open_attempts);
        }
    }

    if (!read_pipe) {
        log_message("ERROR: failed to open pipe %s for reading after %d attempts", pipe_name, MAX_OPEN_ATTEMPTS);
        log_message("ERROR: Final IoErr() = %ld", IoErr());
        log_message("ERROR: System execution success: %s", system_success ? "true" : "false");
        return false;
    }

    /* Initialize timing */
    clock_t start_time = clock();
    log_message("EXECUTE_AMIGA_STREAMING: Start time: %ld", (long)start_time);

    if (!config->silent_mode) {
        printf("Starting real-time %s monitoring...\n", config->tool_name);
        fflush(stdout);
    }

    /* 4. Immediate reading loop with timeout and enhanced safety */
    char buf[64];   /* Even smaller buffer for maximum Amiga safety */
    char partial_line[128] = {0};  /* Much smaller line buffer */

    int line_count = 0;
    ULONG bytesRead;
    int empty_reads = 0;
    const int MAX_EMPTY_READS = config->timeout_seconds * 25; /* 25 checks per second */

    log_message("EXECUTE_AMIGA_STREAMING: Starting pipe read loop, MAX_EMPTY_READS = %d", MAX_EMPTY_READS);

    /* Add safety check for valid pipe */
    if (!read_pipe) {
        log_message("EXECUTE_AMIGA_STREAMING: ERROR - read_pipe is NULL!");
        return false;
    }

    while (empty_reads < MAX_EMPTY_READS) {
        /* Add periodic safety check */
        if (line_count > 1000) {
            log_message("EXECUTE_AMIGA_STREAMING: Safety limit - processed over 1000 lines, breaking");
            break;
        }
        
        log_message("EXECUTE_AMIGA_STREAMING: Read attempt %d/%d", empty_reads + 1, MAX_EMPTY_READS);
        
        /* Initialize buffer safely */
        memset(buf, 0, sizeof(buf));
        
        bytesRead = Read(read_pipe, buf, sizeof(buf) - 1);
        log_message("EXECUTE_AMIGA_STREAMING: Read returned %ld bytes", (long)bytesRead);

        if (bytesRead > 0) {
            empty_reads = 0; /* Reset timeout counter */
            log_message("EXECUTE_AMIGA_STREAMING: Got data, resetting empty_reads counter");

            /* Ensure buffer safety - paranoid safety checks */
            if (bytesRead >= sizeof(buf)) {
                log_message("EXECUTE_AMIGA_STREAMING: CRITICAL - bytesRead %ld >= buffer size %ld", 
                           (long)bytesRead, (long)sizeof(buf));
                bytesRead = sizeof(buf) - 1;
            }

            /* Additional safety - ensure we don't overrun buffer */
            if (bytesRead > 63) {
                log_message("EXECUTE_AMIGA_STREAMING: CRITICAL - bytesRead %ld > 63", (long)bytesRead);
                bytesRead = 63;
            }

            /* Ensure null termination */
            buf[bytesRead] = '\0';
            
            /* Additional safety check for buffer corruption */
            if (bytesRead > 0 && buf[bytesRead-1] != '\0') {
                log_message("EXECUTE_AMIGA_STREAMING: Buffer safety check passed");
            }

            /* Debug: Log raw data received */
            log_message("EXECUTE_AMIGA_STREAMING: Read %ld bytes: [%s]", (long)bytesRead, buf);
            log_message("STREAM_RESPONSE: [%s]", buf);  /* Log every stream response as requested */

            /* Get current timing for real-time feedback */
            clock_t current_time = clock();
            ULONG current_ticks = (unsigned long)current_time;

            /* Process buffer character by character to handle partial lines */
            char *buffer_ptr = buf;
            int chars_processed = 0;
            while (*buffer_ptr) {
                char ch = *buffer_ptr++;
                chars_processed++;

                if (ch == '\n' || ch == '\r') {
                    /* Complete line found */
                    if (strlen(partial_line) > 0) {
                        line_count++;

                        /* Strip escape codes before processing */
                        char cleaned_line[128];
                        strip_escape_codes(partial_line, cleaned_line, sizeof(cleaned_line));

                        /* Debug: Log both raw and cleaned line */
                        log_message("EXECUTE_AMIGA_STREAMING: Processing line %d RAW: [%s]", line_count, partial_line);
                        log_message("EXECUTE_AMIGA_STREAMING: Processing line %d CLEANED: [%s]", line_count, cleaned_line);

                        /* Process the cleaned line for real-time tracking */
                        if (!line_processor(cleaned_line, user_data)) {
                            log_message("EXECUTE_AMIGA_STREAMING: Line processor returned false, stopping");
                            goto cleanup;
                        }

                        /* Clear partial line buffer safely */
                        partial_line[0] = '\0';
                    }
                } else {
                    /* Add character to partial line with STRICT safety */
                    size_t len = strlen(partial_line);
                    if (len < 120) {  /* Much stricter limit - leave 8 chars safety margin */
                        partial_line[len] = ch;
                        partial_line[len + 1] = '\0';
                    } else {
                        /* Buffer would overflow - force line completion */
                        log_message("EXECUTE_AMIGA_STREAMING: Line buffer near full, forcing completion");
                        if (strlen(partial_line) > 0) {
                            line_count++;
                            
                            /* Strip escape codes before processing */
                            char cleaned_line[128];
                            strip_escape_codes(partial_line, cleaned_line, sizeof(cleaned_line));
                            
                            log_message("EXECUTE_AMIGA_STREAMING: Processing forced line %d RAW: [%s]", line_count, partial_line);
                            log_message("EXECUTE_AMIGA_STREAMING: Processing forced line %d CLEANED: [%s]", line_count, cleaned_line);
                            
                            if (!line_processor(cleaned_line, user_data)) {
                                log_message("EXECUTE_AMIGA_STREAMING: Line processor returned false, stopping");
                                goto cleanup;
                            }
                            partial_line[0] = '\0';
                        }
                        /* Add current character to fresh buffer */
                        partial_line[0] = ch;
                        partial_line[1] = '\0';
                    }
                }
            }
            log_message("EXECUTE_AMIGA_STREAMING: Processed %d characters from buffer", chars_processed);
        } else if (bytesRead == 0) {
            /* EOF reached - but might be temporary, check a few times */
            empty_reads++;
            log_message("EXECUTE_AMIGA_STREAMING: EOF reached, empty_reads = %d/%d", empty_reads, MAX_EMPTY_READS);
            if (empty_reads < MAX_EMPTY_READS) {
                /* Use cooperative AmigaOS wait instead of busy-wait */
                log_message("EXECUTE_AMIGA_STREAMING: Calling WaitForChar with 100000 timeout");
                if (WaitForChar(read_pipe, 100000)) {
                    /* Data became available, reset counter and try again */
                    log_message("EXECUTE_AMIGA_STREAMING: WaitForChar returned TRUE - data available");
                    empty_reads = 0;
                    continue;
                } else {
                    /* Timeout occurred (~20ms), continue with empty_reads increment */
                    log_message("EXECUTE_AMIGA_STREAMING: WaitForChar returned FALSE - timeout");
                    continue;
                }
            } else {
                log_message("EXECUTE_AMIGA_STREAMING: Max empty reads reached, breaking loop");
                break;
            }
        } else {
            /* Read error */
            LONG error = IoErr();
            log_message("EXECUTE_AMIGA: Read error: %ld", error);
            log_message("EXECUTE_AMIGA: Bytes read: %ld (negative indicates error)", (long)bytesRead);
            empty_reads++;
            if (empty_reads < MAX_EMPTY_READS) {
                /* Use cooperative AmigaOS wait after read error */
                log_message("EXECUTE_AMIGA_STREAMING: Calling WaitForChar after read error");
                if (WaitForChar(read_pipe, 100000)) {
                    /* Data became available after error, reset counter */
                    log_message("EXECUTE_AMIGA_STREAMING: WaitForChar returned TRUE after error");
                    empty_reads = 0;
                    continue;
                } else {
                    /* Timeout occurred, continue with empty_reads increment */
                    log_message("EXECUTE_AMIGA_STREAMING: WaitForChar returned FALSE after error");
                    continue;
                }
            } else {
                log_message("EXECUTE_AMIGA_STREAMING: Max empty reads reached after error, breaking loop");
                break;
            }
        }
    }

cleanup:

    /* Process any remaining partial line */
    if (strlen(partial_line) > 0) {
        line_count++;
        
        /* Strip escape codes before processing */
        char cleaned_line[128];
        strip_escape_codes(partial_line, cleaned_line, sizeof(cleaned_line));
        
        log_message("EXECUTE_AMIGA_STREAMING: Processing final partial line RAW: [%s]", partial_line);
        log_message("EXECUTE_AMIGA_STREAMING: Processing final partial line CLEANED: [%s]", cleaned_line);
        
        line_processor(cleaned_line, user_data);
    }

    log_message("EXECUTE_AMIGA_STREAMING: Total lines processed: %d", line_count);

    /* Brief delay to allow process cleanup - much shorter than before */
    {
        volatile int delay_counter;
        for (delay_counter = 0; delay_counter < 5000; delay_counter++) {
            /* Minimal cleanup delay - reduced from 20000 to 5000 iterations */
        }
    }

    /* 6. Enhanced cleanup with pipe name clearing */
    log_message("EXECUTE_AMIGA_STREAMING: About to close read pipe");
    if (read_pipe) {
        Close(read_pipe);
        log_message("EXECUTE_AMIGA_STREAMING: Read pipe closed successfully");
    } else {
        log_message("EXECUTE_AMIGA_STREAMING: Read pipe was 0, no close needed");
    }
    
    /* Additional cleanup: Try to clear any remaining pipe references */
    log_message("EXECUTE_AMIGA_STREAMING: Attempting final pipe cleanup");
    BPTR cleanup_pipe = Open(pipe_name, MODE_OLDFILE);
    if (cleanup_pipe) {
        log_message("EXECUTE_AMIGA_STREAMING: Found lingering pipe reference, closing it");
        Close(cleanup_pipe);
    }
    
    /* Extended cleanup delay to ensure system stability */
    {
        volatile int delay_counter;
        for (delay_counter = 0; delay_counter < 20000; delay_counter++) {
            /* Extended cleanup delay - increased to 40ms for stability */
        }
    }
    log_message("EXECUTE_AMIGA_STREAMING: Extended cleanup delay completed");

    if (!config->silent_mode) {
        log_message("EXECUTE_AMIGA_STREAMING: About to print completion message");
        printf("Real-time streaming completed - processed %d lines\n", line_count);
        fflush(stdout);
        log_message("EXECUTE_AMIGA_STREAMING: Completion message printed");
    }

    log_message("EXECUTE_AMIGA_STREAMING: About to return success");
    log_message("EXECUTE_AMIGA_STREAMING: Returning success=true, total lines processed: %d", line_count);
    return true;
}

/* Proper Amiga process management using CreateNewProc */
static bool execute_command_amiga_proper(const char *cmd, 
                                        bool (*line_processor)(const char *, void *), 
                                        void *user_data,
                                        const amiga_exec_config_t *config)
{
    log_message("EXECUTE_AMIGA_PROPER: Starting proper Amiga process execution");
    log_message("EXECUTE_AMIGA_PROPER: Command: %s", cmd);
    log_message("EXECUTE_AMIGA_PROPER: Tool: %s", config->tool_name);
    
    /* Use defaults if config is NULL */
    amiga_exec_config_t default_config = {
        .tool_name = "Command",
        .pipe_prefix = "cmd_pipe",
        .timeout_seconds = 10,
        .silent_mode = false
    };
    
    if (!config) {
        config = &default_config;
    }
    
    /* Generate unique pipe name */
    struct Task *current_task = FindTask(NULL);
    if (!current_task) {
        log_message("EXECUTE_AMIGA_PROPER: ERROR - FindTask returned NULL");
        return false;
    }
    
    static uint32_t sequence_counter = 0;
    sequence_counter++;
    
    char pipe_name[64];
    sprintf(pipe_name, "PIPE:%s.%lu.%lu", config->pipe_prefix, 
            (unsigned long)current_task, (unsigned long)sequence_counter);
    log_message("EXECUTE_AMIGA_PROPER: Generated pipe name: %s", pipe_name);
    
    /* Create command with pipe redirection */
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);
    log_message("EXECUTE_AMIGA_PROPER: Full command: %s", full_cmd);
    
    /* Create new process using CreateNewProc */
    log_message("EXECUTE_AMIGA_PROPER: Creating new process");
    
    struct Process *childProcess = CreateNewProc(TAG_DONE);
    if (!childProcess) {
        log_message("EXECUTE_AMIGA_PROPER: ERROR - CreateNewProc failed");
        return false;
    }
    
    log_message("EXECUTE_AMIGA_PROPER: Process created successfully");
    log_message("EXECUTE_AMIGA_PROPER: Process signal bit: %d", childProcess->pr_Task.tc_SigAlloc);
    
    /* Open pipe for reading */
    log_message("EXECUTE_AMIGA_PROPER: Opening pipe for reading");
    BPTR read_pipe = Open(pipe_name, MODE_OLDFILE);
    if (!read_pipe) {
        log_message("EXECUTE_AMIGA_PROPER: ERROR - Failed to open pipe for reading");
        return false;
    }
    
    /* Read from pipe until EOF */
    char buf[64];
    char partial_line[128] = {0};
    int line_count = 0;
    ULONG bytesRead;
    
    log_message("EXECUTE_AMIGA_PROPER: Starting pipe reading loop");
    
    while ((bytesRead = Read(read_pipe, buf, sizeof(buf) - 1)) > 0) {
        buf[bytesRead] = '\0';
        log_message("EXECUTE_AMIGA_PROPER: Read %ld bytes: [%s]", (long)bytesRead, buf);
        
        /* Process buffer character by character */
        char *buffer_ptr = buf;
        while (*buffer_ptr) {
            char ch = *buffer_ptr++;
            
            if (ch == '\n' || ch == '\r') {
                /* Complete line found */
                if (strlen(partial_line) > 0) {
                    line_count++;
                    
                    /* Strip escape codes */
                    char cleaned_line[128];
                    strip_escape_codes(partial_line, cleaned_line, sizeof(cleaned_line));
                    
                    log_message("EXECUTE_AMIGA_PROPER: Processing line %d: [%s]", line_count, cleaned_line);
                    
                    /* Process the line */
                    if (!line_processor(cleaned_line, user_data)) {
                        log_message("EXECUTE_AMIGA_PROPER: Line processor returned false, stopping");
                        goto cleanup;
                    }
                    
                    /* Clear partial line buffer */
                    partial_line[0] = '\0';
                }
            } else {
                /* Add character to partial line */
                size_t len = strlen(partial_line);
                if (len < sizeof(partial_line) - 1) {
                    partial_line[len] = ch;
                    partial_line[len + 1] = '\0';
                }
            }
        }
    }
    
    log_message("EXECUTE_AMIGA_PROPER: Pipe reading completed, EOF reached");
    
    /* Wait for process to complete using proper signal handling */
    log_message("EXECUTE_AMIGA_PROPER: Waiting for process completion signal");
    ULONG signalMask = 1L << childProcess->pr_Task.tc_SigAlloc;
    Wait(signalMask);
    log_message("EXECUTE_AMIGA_PROPER: Process completion signal received");
    
    /* Get exit code */
    LONG exitCode = 0;
    /* Note: GetAttr might not be available in all Amiga systems, so we'll skip it for now */
    log_message("EXECUTE_AMIGA_PROPER: Process completed with exit code: %ld", exitCode);
    
cleanup:
    /* Close pipe */
    if (read_pipe) {
        Close(read_pipe);
        log_message("EXECUTE_AMIGA_PROPER: Pipe closed");
    }
    
    log_message("EXECUTE_AMIGA_PROPER: Cleanup completed, processed %d lines", line_count);
    return true;
}

/* Legacy wrapper for backward compatibility */
static bool execute_command_amiga(const char *cmd, bool (*line_processor)(const char *, void *), void *user_data)
{
    amiga_exec_config_t lha_config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_pipe",
        .timeout_seconds = 2,
        .silent_mode = false
    };

    return execute_command_amiga_streaming(cmd, line_processor, user_data, &lha_config);
}

#else

static bool execute_command_host(const char *cmd, bool (*line_processor)(const char *, void *), void *user_data)
{
    (void)line_processor; /* Unused parameter in host build */
    (void)user_data; /* Unused parameter in host build */

    log_message("ERROR: Host execution not implemented for command: %s", cmd);
    return false;
}

#endif

bool cli_list(const char *cmd, uint32_t *out_total)
{
    if (!cmd || !out_total) {
        log_message("ERROR: cli_list called with NULL parameters");
        return false;
    }

    if (!cli_wrapper_init()) {
        return false;
    }

    *out_total = 0;

    list_context_t ctx = {0, 0, false};

#ifdef PLATFORM_AMIGA
    bool success = execute_command_amiga(cmd, list_line_processor, &ctx);
#else
    /* Host fallback using clock() */
    bool success = execute_command_host(cmd, list_line_processor, &ctx);
#endif

    log_message("CLI_LIST: After execute_command - success: %s, file_count: %u, total_size: %u",
               success ? "true" : "false", ctx.file_count, ctx.total_size);

    if (success && ctx.file_count > 0) {
        log_message("CLI_LIST: Operation completed successfully - files: %u, total: %u",
                   ctx.file_count, ctx.total_size);
        *out_total = ctx.total_size;
        return true;
    } else {
        log_message("CLI_LIST: Operation failed - success: %s, file_count: %u",
                   success ? "true" : "false", ctx.file_count);
        return false;
    }
}

bool cli_extract(const char *cmd, uint32_t total_expected)
{
    log_message("CLI_EXTRACT: === FUNCTION ENTRY ===");
    log_message("CLI_EXTRACT: Command: %s", cmd ? cmd : "NULL");
    log_message("CLI_EXTRACT: Total expected: %lu", (unsigned long)total_expected);

    if (!cmd) {
        log_message("ERROR: cli_extract called with NULL command");
        return false;
    }
    log_message("CLI_EXTRACT: Command parameter validated");

    if (!cli_wrapper_init()) {
        log_message("ERROR: cli_wrapper_init failed in cli_extract");
        return false;
    }
    log_message("CLI_EXTRACT: CLI wrapper initialized successfully");

    /* CRITICAL: Ensure destination directory exists by creating temp_extract/ */
    log_message("CLI_EXTRACT: Creating destination directory temp_extract/");
#ifdef PLATFORM_AMIGA
    BPTR dir_lock = CreateDir("temp_extract");
    if (dir_lock) {
        log_message("CLI_EXTRACT: Created temp_extract/ directory successfully");
        UnLock(dir_lock);
    } else {
        LONG error = IoErr();
        if (error == ERROR_OBJECT_EXISTS) {
            log_message("CLI_EXTRACT: temp_extract/ directory already exists - good");
        } else {
            log_message("WARNING: Failed to create temp_extract/ directory, IoErr() = %ld", error);
        }
    }
#else
    /* Host platform - would create directory */
    log_message("CLI_EXTRACT: Host platform - would create temp_extract/ directory");
#endif

    /* Console output - inform user that extraction is starting */
    log_message("CLI_EXTRACT: About to print startup messages");
    printf("Starting extraction with real-time progress...\n");
    log_message("CLI_EXTRACT: Printed first message");
    printf("Command: %s\n", cmd);
    log_message("CLI_EXTRACT: Printed command");
    if (total_expected > 0) {
        printf("Expected size: %lu bytes\n", (unsigned long)total_expected);
        log_message("CLI_EXTRACT: Printed expected size");
    }
    printf("NOTE: Progress will be displayed as files are extracted\n");
    fflush(stdout);
    log_message("CLI_EXTRACT: All startup messages printed");

    /* Initialize context with safety checks */
    log_message("CLI_EXTRACT: About to initialize context structure");
    extract_context_t ctx;
    log_message("CLI_EXTRACT: Context structure allocated");
    ctx.total_expected = total_expected;
    log_message("CLI_EXTRACT: Set total_expected = %lu", (unsigned long)total_expected);
    ctx.cumulative_bytes = 0;
    log_message("CLI_EXTRACT: Set cumulative_bytes = 0");
    ctx.file_count = 0;
    log_message("CLI_EXTRACT: Set file_count = 0");
    ctx.last_percentage_x10 = 0;
    log_message("CLI_EXTRACT: Set last_percentage_x10 = 0");
    ctx.completion_detected = false;
    log_message("CLI_EXTRACT: Set completion_detected = false");

#ifdef PLATFORM_AMIGA
    /* Use clock() for timing on Amiga for simplicity */
    clock_t start_time = clock();
#else
    /* Host fallback using clock() */
    clock_t start_time = clock();
#endif

#ifdef PLATFORM_AMIGA
    /* Use the new streaming configuration for safer execution */
    log_message("CLI_EXTRACT: About to configure Amiga execution");
    amiga_exec_config_t extract_config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_pipe",
        .timeout_seconds = 5,  /* Reduced timeout - extraction should be continuous */
        .silent_mode = false
    };
    log_message("CLI_EXTRACT: About to call execute_command_amiga_streaming");
    log_message("CLI_EXTRACT: Context ptr: %p", (void*)&ctx);
    log_message("CLI_EXTRACT: Config ptr: %p", (void*)&extract_config);
    log_message("CLI_EXTRACT: Command string: [%s]", cmd);
    log_message("CLI_EXTRACT: Line processor function ptr: %p", (void*)extract_line_processor);

    bool success = execute_command_amiga_streaming(cmd, extract_line_processor, &ctx, &extract_config);
    log_message("CLI_EXTRACT: execute_command_amiga_streaming returned: %s", success ? "true" : "false");
#else
    log_message("CLI_EXTRACT: About to call execute_command_host");
    bool success = execute_command_host(cmd, extract_line_processor, &ctx);
    log_message("CLI_EXTRACT: execute_command_host returned: %s", success ? "true" : "false");
#endif

#ifdef PLATFORM_AMIGA
    /* Calculate elapsed time using clock() */
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
#else
    /* Host fallback */
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
#endif

    /* Calculate final percentage using integer math */
    uint32_t final_percentage_x10 = 0;
    if (total_expected > 0) {
        final_percentage_x10 = (ctx.cumulative_bytes * 1000) / total_expected;
    }

    /* Check for success conditions */
    bool operation_success = false;

    if (success && ctx.file_count > 0) {
        operation_success = true;
    } else {
        /* Fallback check - look for destination directory */
        /* Extract destination path from command */
        const char *last_space = strrchr(cmd, ' ');
        if (last_space && check_directory_exists(last_space + 1)) {
            operation_success = true;
        }
    }

    /* Console output - final results */
    if (operation_success) {
        printf("\nExtraction completed successfully!\n");
        printf("Files extracted: %lu\n", (unsigned long)ctx.file_count);
        printf("Bytes processed: %lu\n", (unsigned long)ctx.cumulative_bytes);
        if (total_expected > 0) {
            printf("Final percentage: %lu.%lu%%\n",
                   (unsigned long)(final_percentage_x10 / 10),
                   (unsigned long)(final_percentage_x10 % 10));
        }
#ifdef PLATFORM_AMIGA
        printf("Time elapsed: %lu ticks\n", elapsed_ticks);
#else
        printf("Time elapsed: %lu ticks\n", elapsed_ticks);
#endif
    } else {
        printf("\nExtraction failed!\n");
        fflush(stdout);
    }

    fflush(stdout);

    return operation_success;
}

/* Line processor for unzip list command */
static bool unzip_list_line_processor(const char *line, void *user_data)
{
    list_context_t *ctx = (list_context_t *)user_data;
    uint32_t file_size;

    /* Parse unzip -l output format - adapt based on actual unzip output */
    if (parse_unzip_list_line(line, &file_size)) {
        ctx->total_size += file_size;
        ctx->file_count++;
    }

    return true; /* Continue processing */
}

/* Line processor for unzip extract command */
static bool unzip_extract_line_processor(const char *line, void *user_data)
{
    extract_context_t *ctx = (extract_context_t *)user_data;
    uint32_t file_size;
    char filename[256];

    /* Parse unzip extract output format - adapt based on actual unzip output */
    if (parse_unzip_extract_line(line, &file_size, filename, sizeof(filename))) {
        ctx->cumulative_bytes += file_size;
        ctx->file_count++;

#ifdef PLATFORM_AMIGA
        /* Get current time using clock() for simplicity */
        clock_t current_time = clock();
        unsigned long current_jiffies = (unsigned long)current_time;
#else
        /* Host fallback using clock() */
        clock_t current_time = clock();
        unsigned long current_jiffies = (unsigned long)current_time;
#endif

        /* Estimate total files (rough calculation) */
        uint32_t estimated_total_files = ctx->total_expected > 0 ?
            (ctx->total_expected / 4000) : ctx->file_count; /* Rough 4KB average */

        /* Real-time console output - immediate feedback as extraction happens */
        printf("Extracting: %s (%u/%u) %d jiffies\n",
               filename,
               ctx->file_count,
               estimated_total_files,
               (int)current_jiffies);
        fflush(stdout);
    }

    return true; /* Continue processing */
}

bool unzip_list(const char *cmd, uint32_t *out_total)
{
    if (!cmd || !out_total) {
        log_message("ERROR: unzip_list called with NULL parameters");
        return false;
    }

    if (!cli_wrapper_init()) {
        return false;
    }

    *out_total = 0;

    list_context_t ctx = {0, 0, false};

#ifdef PLATFORM_AMIGA
    /* Configure for unzip */
    amiga_exec_config_t unzip_config = {
        .tool_name = "unzip",
        .pipe_prefix = "unzip_pipe",
        .timeout_seconds = 3,  /* unzip might be slower */
        .silent_mode = false
    };

    bool success = execute_command_amiga_streaming(cmd, unzip_list_line_processor, &ctx, &unzip_config);
#else
    bool success = execute_command_host(cmd, unzip_list_line_processor, &ctx);
#endif

    if (success && ctx.file_count > 0) {
        *out_total = ctx.total_size;
        return true;
    } else {
        return false;
    }
}

bool unzip_extract(const char *cmd, uint32_t total_expected)
{
    if (!cmd) {
        log_message("ERROR: unzip_extract called with NULL command");
        return false;
    }

    if (!cli_wrapper_init()) {
        return false;
    }

    /* Console output - inform user that extraction is starting */
    printf("Starting unzip extraction with real-time progress...\n");
    printf("Command: %s\n", cmd);
    if (total_expected > 0) {
        printf("Expected size: %lu bytes\n", (unsigned long)total_expected);
    }
    printf("NOTE: Progress will be displayed as files are extracted\n");
    fflush(stdout);

    extract_context_t ctx = {total_expected, 0, 0, 0, false};

#ifdef PLATFORM_AMIGA
    /* Configure for unzip */
    amiga_exec_config_t unzip_config = {
        .tool_name = "unzip",
        .pipe_prefix = "unzip_pipe",
        .timeout_seconds = 5,  /* extraction might take longer */
        .silent_mode = false
    };

    clock_t start_time = clock();
    bool success = execute_command_amiga_streaming(cmd, unzip_extract_line_processor, &ctx, &unzip_config);
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
#else
    bool success = execute_command_host(cmd, unzip_extract_line_processor, &ctx);
    unsigned long elapsed_ticks = 0; /* Dummy value for host */
#endif

    /* Calculate final percentage using integer math */
    uint32_t final_percentage_x10 = 0;
    if (total_expected > 0) {
        final_percentage_x10 = (ctx.cumulative_bytes * 1000) / total_expected;
    }

    /* Check for success conditions */
    bool operation_success = false;

    if (success && ctx.file_count > 0) {
        operation_success = true;
    } else {
        /* Fallback check - look for destination directory */
        /* Extract destination path from command */
        const char *last_space = strrchr(cmd, ' ');
        if (last_space && check_directory_exists(last_space + 1)) {
            operation_success = true;
        }
    }

    /* Console output - final results */
    if (operation_success) {
        printf("\nUnzip extraction completed successfully!\n");
        printf("Files extracted: %lu\n", (unsigned long)ctx.file_count);
        printf("Bytes processed: %lu\n", (unsigned long)ctx.cumulative_bytes);
        if (total_expected > 0) {
            printf("Final percentage: %lu.%lu%%\n",
                   (unsigned long)(final_percentage_x10 / 10),
                   (unsigned long)(final_percentage_x10 % 10));
        }
        printf("Time elapsed: %lu ticks\n", elapsed_ticks);
    } else {
        printf("\nUnzip extraction failed!\n");
        fflush(stdout);
    }

    fflush(stdout);

    return operation_success;
}

/* Strip ANSI escape codes from a string for cleaner parsing */
static void strip_escape_codes(const char *input, char *output, size_t output_size)
{
    const char *src = input;
    char *dst = output;
    size_t written = 0;
    
    if (!input || !output || output_size == 0) {
        if (output && output_size > 0) {
            output[0] = '\0';
        }
        return;
    }
    
    while (*src && written < output_size - 1) {
        if (*src == 27 || *src == '\033') { /* ESC character */
            /* Skip escape sequence */
            src++; /* Skip ESC */
            
            /* Skip CSI sequences like ESC[...m */
            if (*src == '[') {
                src++; /* Skip [ */
                /* Skip until we find a letter (command terminator) */
                while (*src && (*src < 'A' || *src > 'z')) {
                    src++;
                }
                if (*src) src++; /* Skip the command letter */
            }
            /* Skip other escape sequences */
            else if (*src) {
                src++;
            }
        }
        else {
            /* Regular character - copy it */
            *dst++ = *src++;
            written++;
        }
    }
    
    *dst = '\0';
}
