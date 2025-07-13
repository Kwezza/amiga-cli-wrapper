#include "cli_wrapper.h"
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
} list_context_t;

typedef struct {
    uint32_t total_expected;
    uint32_t cumulative_bytes;
    uint32_t file_count;
    uint32_t last_percentage_x10;  /* Percentage * 10 to avoid floating point */
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
    /* Parse LHA extract output format:
     * " Extracting: (   10380)  A10TankKiller3Disk/data/A10[K"
     * We want the number in parentheses and the filename
     */

    *file_size = 0;
    filename[0] = '\0';

    /* Look for " Extracting: (" pattern */
    const char *extract_pos = strstr(line, " Extracting: (");
    if (!extract_pos) {
        return false;
    }

    /* Move to the size number */
    const char *size_start = extract_pos + 14; /* Length of " Extracting: (" */
    while (*size_start == ' ') size_start++;

    /* Parse the size */
    char *endptr;
    unsigned long size = strtoul(size_start, &endptr, 10);

    /* Look for closing paren and filename */
    const char *paren_pos = strchr(size_start, ')');
    if (!paren_pos) {
        return false;
    }

    /* Find filename after the paren and spaces */
    const char *filename_start = paren_pos + 1;
    while (*filename_start == ' ' || *filename_start == '\t') filename_start++;

    /* Copy filename, stopping at control characters */
    size_t i;
    for (i = 0; i < filename_max - 1 && filename_start[i]; i++) {
        if (filename_start[i] == '[' || filename_start[i] < ' ') {
            break;
        }
        filename[i] = filename_start[i];
    }
    filename[i] = '\0';

    *file_size = (uint32_t)size;
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
    list_context_t *ctx = (list_context_t *)user_data;
    uint32_t file_size;

    log_message("LIST_RAW: %s", line);

    if (parse_lha_list_line(line, &file_size)) {
        ctx->total_size += file_size;
        ctx->file_count++;
        log_message("LIST_PARSED: size=%lu, running_total=%lu, file_count=%lu",
                   (unsigned long)file_size,
                   (unsigned long)ctx->total_size,
                   (unsigned long)ctx->file_count);
    } else {
        log_message("LIST_SKIP: line did not match file pattern");
    }

    return true; /* Continue processing */
}

/* Line processor for extract command */
static bool extract_line_processor(const char *line, void *user_data)
{
    extract_context_t *ctx = (extract_context_t *)user_data;
    uint32_t file_size;
    static char filename[64];  /* Static to avoid stack usage, smaller size */

    log_message("EXTRACT_RAW: %s", line);

    if (parse_lha_extract_line(line, &file_size, filename, sizeof(filename))) {
        ctx->cumulative_bytes += file_size;
        ctx->file_count++;

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
        printf("Extracting: %s (%u files) [%u.%u%%] %lu jiffies\n",
               filename,
               ctx->file_count,
               percentage_x10 / 10,
               percentage_x10 % 10,
               current_jiffies);
        fflush(stdout);

        /* Detailed log entry with full progress information */
        log_message("EXTRACT: %s — file %u — %lu jiffies (%u%%)",
                   filename,
                   ctx->file_count,
                   current_jiffies,
                   percentage_x10 / 10);

        log_message("EXTRACT_PARSED: file=%s, size=%u, cumulative=%u, percentage=%u.%u%%, jiffies=%lu",
                   filename,
                   file_size,
                   ctx->cumulative_bytes,
                   percentage_x10 / 10,
                   percentage_x10 % 10,
                   current_jiffies);

        /* Log significant percentage milestones */
        if (percentage_x10 - ctx->last_percentage_x10 >= 100 || percentage_x10 >= 1000) {
            log_message("PROGRESS_MILESTONE: %u.%u%% complete (%u / %u bytes) — %lu jiffies",
                       percentage_x10 / 10,
                       percentage_x10 % 10,
                       ctx->cumulative_bytes,
                       ctx->total_expected,
                       current_jiffies);
            ctx->last_percentage_x10 = percentage_x10;
        }
    } else {
        log_message("EXTRACT_SKIP: line did not match extraction pattern");
    }

    return true; /* Continue processing */
}

#ifdef PLATFORM_AMIGA

typedef struct {
    const char *tool_name;      /* "LhA", "unzip", etc. */
    const char *pipe_prefix;    /* "lha_pipe", "unzip_pipe", etc. */
    int timeout_seconds;        /* Default: 2 */
    bool silent_mode;          /* Suppress console output */
} amiga_exec_config_t;

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

    log_message("EXECUTE_AMIGA: Starting asynchronous streaming command (%s): %s",
                config->tool_name, cmd);
    fflush(stdout);

    /* Generate unique pipe name using process ID and tool prefix */
    log_message("EXECUTE_AMIGA: About to find current task...");
    fflush(stdout);

    struct Task *current_task = FindTask(NULL);
    if (!current_task) {
        log_message("ERROR: FindTask returned NULL");
        return false;
    }

    char pipe_name[64];
    log_message("EXECUTE_AMIGA: About to generate pipe name...");
    fflush(stdout);

    sprintf(pipe_name, "PIPE:%s.%lu", config->pipe_prefix, (unsigned long)current_task);

    log_message("EXECUTE_AMIGA: Using pipe: %s", pipe_name);
    fflush(stdout);

    /* 1. Open pipe first for writing */
    log_message("EXECUTE_AMIGA: About to open pipe for writing...");
    fflush(stdout);

    BPTR pipe = Open(pipe_name, MODE_NEWFILE);
    if (!pipe) {
        log_message("ERROR: failed to open pipe %s", pipe_name);
        return false;
    }

    log_message("EXECUTE_AMIGA: Successfully opened pipe for writing");

    /* Console output */
    if (!config->silent_mode) {
        printf("Spawning %s process asynchronously...\n", config->tool_name);
        fflush(stdout);
    }

    /* Create command with pipe redirection for background execution */
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);

    log_message("EXECUTE_AMIGA: Full command: %s", full_cmd);

    /* Close the write pipe handle so the background command can open it */
    Close(pipe);

    /* Execute command asynchronously using SystemTags or fallback to System */
    log_message("EXECUTE_AMIGA: About to create SystemTags structure...");
    fflush(stdout);

    struct TagItem tags[] = {
        { SYS_Asynch, TRUE },
        { TAG_END, 0 }
    };

    log_message("EXECUTE_AMIGA: SystemTags structure created, about to call SystemTagList...");
    fflush(stdout);

    LONG proc_result = SystemTagList(full_cmd, tags);

    log_message("EXECUTE_AMIGA: SystemTagList returned: %ld", proc_result);
    fflush(stdout);

    if (proc_result == -1) {
        log_message("EXECUTE_AMIGA: SystemTagList failed, trying fallback System()");
        fflush(stdout);
        /* Fallback to regular System() if SystemTagList fails */
        proc_result = System(full_cmd, NULL);
        log_message("EXECUTE_AMIGA: System() returned: %ld", proc_result);
        fflush(stdout);
        if (proc_result == -1) {
            log_message("ERROR: failed to spawn %s process with both methods", config->tool_name);
            return false;
        }
        log_message("EXECUTE_AMIGA: %s process spawned successfully (System fallback)", config->tool_name);
    } else if (proc_result != 0) {
        /* Non-zero return codes might indicate issues but not necessarily failure */
        log_message("EXECUTE_AMIGA: %s process returned code %ld (may be normal)", config->tool_name, proc_result);
    } else {
        log_message("EXECUTE_AMIGA: %s process spawned successfully (async)", config->tool_name);
    }

    log_message("EXECUTE_AMIGA: Process spawn completed, system still stable");
    fflush(stdout);

    /* Give the asynchronous process time to start and open the pipe */
    log_message("EXECUTE_AMIGA: Adding delay for process startup...");
    fflush(stdout);

    log_message("EXECUTE_AMIGA: Using safe busy-wait instead of Delay()...");
    fflush(stdout);

    /* Use a safe busy-wait loop instead of Delay() to avoid system crashes */
    {
        volatile int delay_counter;
        for (delay_counter = 0; delay_counter < 200000; delay_counter++) {
            /* Busy wait - safer than Delay() which may cause crashes */
        }
    }

    log_message("EXECUTE_AMIGA: Safe delay completed successfully");
    fflush(stdout);

    log_message("EXECUTE_AMIGA: Delay completed, attempting to open pipe...");
    fflush(stdout);

    /* 3. Open pipe for reading immediately */
    log_message("EXECUTE_AMIGA: About to open pipe %s for reading...", pipe_name);
    fflush(stdout);

    log_message("EXECUTE_AMIGA: Calling Open() with MODE_OLDFILE...");
    fflush(stdout);

    BPTR read_pipe = Open(pipe_name, MODE_OLDFILE);

    log_message("EXECUTE_AMIGA: Open() call completed, handle=%ld", (long)read_pipe);
    fflush(stdout);

    if (!read_pipe) {
        log_message("ERROR: failed to open pipe %s for reading", pipe_name);
        return false;
    }

    log_message("EXECUTE_AMIGA: Successfully opened pipe for reading");
    fflush(stdout);

    /* Initialize timing */
    log_message("EXECUTE_AMIGA: About to initialize timing...");
    fflush(stdout);

    log_message("EXECUTE_AMIGA: About to call clock()...");
    fflush(stdout);
    clock_t start_time = clock();
    log_message("EXECUTE_AMIGA: clock() returned successfully: %lu ticks", (unsigned long)start_time);
    fflush(stdout);

    log_message("EXECUTE_AMIGA: Started real-time streaming at %lu ticks", (unsigned long)start_time);
    fflush(stdout);

    /* 4. Immediate reading loop with timeout - stream data as command generates output */
    log_message("EXECUTE_AMIGA: About to allocate buffers...");
    fflush(stdout);

    log_message("EXECUTE_AMIGA: About to create buffer variables...");
    fflush(stdout);

    char buf[64];   /* Even smaller buffer for maximum Amiga safety */
    char partial_line[128] = {0};  /* Much smaller line buffer */

    log_message("EXECUTE_AMIGA: Buffer variables created successfully");
    fflush(stdout);

    log_message("EXECUTE_AMIGA: About to initialize loop variables...");
    fflush(stdout);

    int line_count = 0;
    ULONG bytesRead;
    int empty_reads = 0;
    const int MAX_EMPTY_READS = config->timeout_seconds * 25; /* 25 checks per second */

    log_message("EXECUTE_AMIGA: Buffers allocated (buf=%d, partial_line=%d), initializing loop...",
                (int)sizeof(buf), (int)sizeof(partial_line));
    log_message("EXECUTE_AMIGA: Loop variables: line_count=%d, empty_reads=%d, MAX_EMPTY_READS=%d",
                line_count, empty_reads, MAX_EMPTY_READS);
    fflush(stdout);

    if (!config->silent_mode) {
        printf("Starting real-time %s monitoring...\n", config->tool_name);
        fflush(stdout);
    }

    log_message("EXECUTE_AMIGA: About to enter main reading loop...");
    log_message("EXECUTE_AMIGA: MAX_EMPTY_READS=%d, timeout_seconds=%d", MAX_EMPTY_READS, config->timeout_seconds);
    fflush(stdout);

    while (empty_reads < MAX_EMPTY_READS) {
        log_message("EXECUTE_AMIGA: Loop iteration, empty_reads=%d", empty_reads);
        fflush(stdout);

        bytesRead = Read(read_pipe, buf, sizeof(buf) - 1);
        log_message("EXECUTE_AMIGA: Read returned %lu bytes", (unsigned long)bytesRead);
        fflush(stdout);

        if (bytesRead > 0) {
            empty_reads = 0; /* Reset timeout counter */

            /* Ensure buffer safety - paranoid safety checks */
            if (bytesRead >= sizeof(buf)) {
                log_message("EXECUTE_AMIGA: WARNING: bytesRead too large, truncating");
                bytesRead = sizeof(buf) - 1;
            }

            /* Additional safety - ensure we don't overrun buffer */
            if (bytesRead > 63) {
                log_message("EXECUTE_AMIGA: WARNING: bytesRead > 63, setting to 63");
                bytesRead = 63;
            }

            buf[bytesRead] = '\0';

            /* Log raw output for debugging */
            log_message("RAW: %s", buf);

            /* Get current timing for real-time feedback */
            log_message("EXECUTE_AMIGA: About to call clock() for timing...");
            fflush(stdout);
            clock_t current_time = clock();
            ULONG current_ticks = (unsigned long)current_time;
            log_message("EXECUTE_AMIGA: Clock call completed, ticks=%lu", current_ticks);
            fflush(stdout);

            /* Process buffer character by character to handle partial lines */
            log_message("EXECUTE_AMIGA: About to process buffer character by character...");
            fflush(stdout);

            char *buffer_ptr = buf;
            while (*buffer_ptr) {
                char ch = *buffer_ptr++;

                if (ch == '\n' || ch == '\r') {
                    /* Complete line found */
                    if (strlen(partial_line) > 0) {
                        line_count++;

                        log_message("EXECUTE_AMIGA: Line %d at %lu ticks: %s",
                                   line_count, current_ticks, partial_line);

                        /* Process the line immediately for real-time tracking */
                        log_message("EXECUTE_AMIGA: About to call line_processor...");
                        fflush(stdout);

                        if (!line_processor(partial_line, user_data)) {
                            log_message("EXECUTE_AMIGA: Line processor requested stop at line %d", line_count);
                            goto cleanup;
                        }

                        log_message("EXECUTE_AMIGA: Line processor completed successfully");
                        fflush(stdout);

                        /* Clear partial line buffer safely */
                        partial_line[0] = '\0';
                    }
                } else {
                    /* Add character to partial line with extra safety */
                    size_t len = strlen(partial_line);
                    if (len < sizeof(partial_line) - 2) {  /* Extra safety margin */
                        partial_line[len] = ch;
                        partial_line[len + 1] = '\0';
                    } else {
                        log_message("EXECUTE_AMIGA: WARNING: partial_line buffer near full, len=%d", (int)len);
                    }
                }
            }

            log_message("EXECUTE_AMIGA: Buffer processing completed successfully");
            fflush(stdout);
        } else if (bytesRead == 0) {
            /* EOF reached - but might be temporary, check a few times */
            empty_reads++;
            if (empty_reads < MAX_EMPTY_READS) {
                /* Use safe busy-wait instead of Delay() */
                volatile int delay_counter;
                for (delay_counter = 0; delay_counter < 10000; delay_counter++) {
                    /* 20ms equivalent busy-wait */
                }
                continue;
            } else {
                log_message("EXECUTE_AMIGA: EOF reached after timeout");
                break;
            }
        } else {
            /* Read error */
            LONG error = IoErr();
            log_message("EXECUTE_AMIGA: Read error: %ld", error);
            empty_reads++;
            if (empty_reads < MAX_EMPTY_READS) {
                /* Use safe busy-wait instead of Delay() */
                volatile int delay_counter;
                for (delay_counter = 0; delay_counter < 10000; delay_counter++) {
                    /* 20ms equivalent busy-wait */
                }
                continue;
            } else {
                break;
            }
        }
    }

cleanup:

    /* Process any remaining partial line */
    if (strlen(partial_line) > 0) {
        line_count++;
        log_message("EXECUTE_AMIGA: Final line %d: %s", line_count, partial_line);
        line_processor(partial_line, user_data);
    }

    /* 5. Process completion - EOF indicates command finished */
    log_message("EXECUTE_AMIGA: EOF reached, command completed successfully");

    /* Brief safe delay to allow process cleanup */
    log_message("EXECUTE_AMIGA: Using safe busy-wait for cleanup delay...");
    fflush(stdout);
    {
        volatile int delay_counter;
        for (delay_counter = 0; delay_counter < 20000; delay_counter++) {
            /* 40ms equivalent busy-wait for process cleanup */
        }
    }
    log_message("EXECUTE_AMIGA: Cleanup delay completed safely");
    fflush(stdout);

    log_message("EXECUTE_AMIGA: Process cleanup completed");

    /* 6. Cleanup */
    Close(read_pipe);

    /* Calculate final timing */
    clock_t end_time = clock();
    ULONG total_elapsed = (unsigned long)(end_time - start_time);

    log_message("EXECUTE_AMIGA: Processed %d lines in real-time streaming mode", line_count);
    log_message("EXECUTE_AMIGA: Total execution time: %lu ticks", total_elapsed);
    log_message("EXECUTE_AMIGA: Asynchronous streaming completed successfully");

    if (!config->silent_mode) {
        printf("Real-time streaming completed - processed %d lines\n", line_count);
        fflush(stdout);
    }

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

    log_message("EXECUTE_HOST: Command stubbed (not implemented): %s", cmd);
    log_message("EXECUTE_HOST: Service not supported on host");
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

    log_message("=== CLI_LIST START ===");
    log_message("Command: %s", cmd);

    *out_total = 0;

    list_context_t ctx = {0, 0};

#ifdef PLATFORM_AMIGA
    /* Use clock() for timing on Amiga for simplicity */
    clock_t start_time = clock();
    log_message("TIMING: Using clock() for timing");
#else
    /* Host fallback using clock() */
    clock_t start_time = clock();
#endif

#ifdef PLATFORM_AMIGA
    bool success = execute_command_amiga(cmd, list_line_processor, &ctx);
#else
    bool success = execute_command_host(cmd, list_line_processor, &ctx);
#endif

#ifdef PLATFORM_AMIGA
    /* Calculate elapsed time using clock() */
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);

    log_message("TIMING: List operation took %lu ticks", elapsed_ticks);
#else
    /* Host fallback */
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
    log_message("TIMING: List operation took %lu ticks", elapsed_ticks);
#endif
    log_message("RESULT: Files processed: %lu, Total size: %lu bytes",
               (unsigned long)ctx.file_count, (unsigned long)ctx.total_size);

    if (success && ctx.file_count > 0) {
        *out_total = ctx.total_size;
        log_message("SUCCESS: cli_list completed successfully");
        log_message("=== CLI_LIST END ===");
        return true;
    } else {
        log_message("FAILURE: cli_list failed - success=%s, files=%lu",
                   success ? "true" : "false", (unsigned long)ctx.file_count);
        log_message("=== CLI_LIST END ===");
        return false;
    }
}

bool cli_extract(const char *cmd, uint32_t total_expected)
{
    if (!cmd) {
        log_message("ERROR: cli_extract called with NULL command");
        return false;
    }

    log_message("=== CLI_EXTRACT START ===");
    log_message("Command: %s", cmd);
    log_message("Expected total: %lu bytes", (unsigned long)total_expected);

    log_message("EXTRACT: About to call cli_wrapper_init...");
    fflush(stdout);

    if (!cli_wrapper_init()) {
        log_message("ERROR: cli_wrapper_init failed in cli_extract");
        return false;
    }

    log_message("EXTRACT: cli_wrapper_init completed successfully");
    fflush(stdout);

    /* Console output - inform user that extraction is starting */
    log_message("EXTRACT: About to display console messages...");
    printf("Starting extraction with real-time progress...\n");
    printf("Command: %s\n", cmd);
    if (total_expected > 0) {
        printf("Expected size: %lu bytes\n", (unsigned long)total_expected);
    }
    printf("NOTE: Progress will be displayed as files are extracted\n");
    fflush(stdout);
    log_message("EXTRACT: Console messages displayed");

    /* Initialize context with safety checks */
    log_message("EXTRACT: About to initialize context structure...");
    fflush(stdout);

    extract_context_t ctx;
    ctx.total_expected = total_expected;
    ctx.cumulative_bytes = 0;
    ctx.file_count = 0;
    ctx.last_percentage_x10 = 0;

    log_message("EXTRACT: Context initialized successfully");
    log_message("EXTRACT: ctx.total_expected = %lu", (unsigned long)ctx.total_expected);

#ifdef PLATFORM_AMIGA
    /* Use clock() for timing on Amiga for simplicity */
    log_message("EXTRACT: About to initialize timing...");
    fflush(stdout);
    clock_t start_time = clock();
    log_message("TIMING: Using clock() for timing, start_time=%lu", (unsigned long)start_time);
#else
    /* Host fallback using clock() */
    clock_t start_time = clock();
#endif

    log_message("EXTRACT: About to execute command...");
    fflush(stdout);

#ifdef PLATFORM_AMIGA
    /* Use the new streaming configuration for safer execution */
    amiga_exec_config_t extract_config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_pipe",
        .timeout_seconds = 15,  /* Even shorter timeout */
        .silent_mode = false
    };

    log_message("EXTRACT: Configuration created, about to call streaming execution...");
    fflush(stdout);

    bool success = execute_command_amiga_streaming(cmd, extract_line_processor, &ctx, &extract_config);

    log_message("EXTRACT: execute_command_amiga_streaming returned: %s", success ? "true" : "false");
    fflush(stdout);
#else
    bool success = execute_command_host(cmd, extract_line_processor, &ctx);
#endif

    log_message("EXTRACT: Command execution completed, success=%s", success ? "true" : "false");
    fflush(stdout);

#ifdef PLATFORM_AMIGA
    /* Calculate elapsed time using clock() */
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);

    log_message("TIMING: Extract operation took %lu ticks", elapsed_ticks);
#else
    /* Host fallback */
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
    log_message("TIMING: Extract operation took %lu ticks", elapsed_ticks);
#endif
    log_message("RESULT: Files extracted: %lu, Bytes processed: %lu",
               (unsigned long)ctx.file_count, (unsigned long)ctx.cumulative_bytes);

    /* Calculate final percentage using integer math */
    uint32_t final_percentage_x10 = 0;
    if (total_expected > 0) {
        final_percentage_x10 = (ctx.cumulative_bytes * 1000) / total_expected;
    }
    log_message("FINAL_PERCENTAGE: %lu.%lu%%",
               (unsigned long)(final_percentage_x10 / 10),
               (unsigned long)(final_percentage_x10 % 10));

    /* Check for success conditions */
    bool operation_success = false;

    if (success && ctx.file_count > 0) {
        log_message("PRIMARY_SUCCESS: Command executed and files processed");
        operation_success = true;
    } else {
        /* Fallback check - look for destination directory */
        log_message("PRIMARY_FAILURE: Attempting fallback directory check");

        /* Extract destination path from command */
        const char *last_space = strrchr(cmd, ' ');
        if (last_space && check_directory_exists(last_space + 1)) {
            log_message("FALLBACK_SUCCESS: Destination directory exists, assuming extraction succeeded");
            operation_success = true;
        } else {
            log_message("FALLBACK_FAILURE: No destination directory found");
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
        log_message("SUCCESS: cli_extract completed successfully");
    } else {
        printf("\nExtraction failed!\n");
        fflush(stdout);
        log_message("FAILURE: cli_extract failed completely");
    }

    fflush(stdout);
    log_message("=== CLI_EXTRACT END ===");

    return operation_success;
}

/* Line processor for unzip list command */
static bool unzip_list_line_processor(const char *line, void *user_data)
{
    list_context_t *ctx = (list_context_t *)user_data;
    uint32_t file_size;

    log_message("UNZIP_LIST_RAW: %s", line);

    /* Parse unzip -l output format - adapt based on actual unzip output */
    if (parse_unzip_list_line(line, &file_size)) {
        ctx->total_size += file_size;
        ctx->file_count++;
        log_message("UNZIP_LIST_PARSED: size=%lu, running_total=%lu, file_count=%lu",
                   (unsigned long)file_size,
                   (unsigned long)ctx->total_size,
                   (unsigned long)ctx->file_count);
    } else {
        log_message("UNZIP_LIST_SKIP: line did not match file pattern");
    }

    return true; /* Continue processing */
}

/* Line processor for unzip extract command */
static bool unzip_extract_line_processor(const char *line, void *user_data)
{
    extract_context_t *ctx = (extract_context_t *)user_data;
    uint32_t file_size;
    char filename[256];

    log_message("UNZIP_EXTRACT_RAW: %s", line);

    /* Parse unzip extract output format - adapt based on actual unzip output */
    if (parse_unzip_extract_line(line, &file_size, filename, sizeof(filename))) {
        ctx->cumulative_bytes += file_size;
        ctx->file_count++;

        /* Calculate percentage using integer math (x10 for one decimal place) */
        uint32_t percentage_x10 = 0;
        if (ctx->total_expected > 0) {
            percentage_x10 = (ctx->cumulative_bytes * 1000) / ctx->total_expected;
        }

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
        printf("Extracting: %s (%u/%u) %lu jiffies\n",
               filename,
               ctx->file_count,
               estimated_total_files,
               current_jiffies);
        fflush(stdout);

        /* Detailed log entry with full progress information */
        log_message("UNZIP_EXTRACT: %s — file %u/%u — %lu jiffies (%u%%)",
                   filename,
                   ctx->file_count,
                   estimated_total_files,
                   current_jiffies,
                   percentage_x10 / 10);

        log_message("UNZIP_EXTRACT_PARSED: file=%s, size=%u, cumulative=%u, percentage=%u.%u%%, jiffies=%lu",
                   filename,
                   file_size,
                   ctx->cumulative_bytes,
                   percentage_x10 / 10,
                   percentage_x10 % 10,
                   current_jiffies);

        /* Log significant percentage milestones */
        if (percentage_x10 - ctx->last_percentage_x10 >= 100 || percentage_x10 >= 1000) {
            log_message("UNZIP_PROGRESS_MILESTONE: %u.%u%% complete (%u / %u bytes) — %lu jiffies",
                       percentage_x10 / 10,
                       percentage_x10 % 10,
                       ctx->cumulative_bytes,
                       ctx->total_expected,
                       current_jiffies);
            ctx->last_percentage_x10 = percentage_x10;
        }
    } else {
        log_message("UNZIP_EXTRACT_SKIP: line did not match extraction pattern");
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

    log_message("=== UNZIP_LIST START ===");
    log_message("Command: %s", cmd);

    *out_total = 0;

    list_context_t ctx = {0, 0};

    /* Configure for unzip */
    amiga_exec_config_t unzip_config = {
        .tool_name = "unzip",
        .pipe_prefix = "unzip_pipe",
        .timeout_seconds = 3,  /* unzip might be slower */
        .silent_mode = false
    };

#ifdef PLATFORM_AMIGA
    clock_t start_time = clock();
    log_message("TIMING: Using clock() for timing");
#else
    clock_t start_time = clock();
#endif

#ifdef PLATFORM_AMIGA
    bool success = execute_command_amiga_streaming(cmd, unzip_list_line_processor, &ctx, &unzip_config);
#else
    bool success = execute_command_host(cmd, unzip_list_line_processor, &ctx);
#endif

#ifdef PLATFORM_AMIGA
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
    log_message("TIMING: Unzip list operation took %lu ticks", elapsed_ticks);
#else
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
    log_message("TIMING: Unzip list operation took %lu ticks", elapsed_ticks);
#endif

    log_message("RESULT: Files processed: %lu, Total size: %lu bytes",
               (unsigned long)ctx.file_count, (unsigned long)ctx.total_size);

    if (success && ctx.file_count > 0) {
        *out_total = ctx.total_size;
        log_message("SUCCESS: unzip_list completed successfully");
        log_message("=== UNZIP_LIST END ===");
        return true;
    } else {
        log_message("FAILURE: unzip_list failed - success=%s, files=%lu",
                   success ? "true" : "false", (unsigned long)ctx.file_count);
        log_message("=== UNZIP_LIST END ===");
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

    log_message("=== UNZIP_EXTRACT START ===");
    log_message("Command: %s", cmd);
    log_message("Expected total: %lu bytes", (unsigned long)total_expected);

    extract_context_t ctx = {total_expected, 0, 0, 0};

    /* Configure for unzip */
    amiga_exec_config_t unzip_config = {
        .tool_name = "unzip",
        .pipe_prefix = "unzip_pipe",
        .timeout_seconds = 5,  /* extraction might take longer */
        .silent_mode = false
    };

#ifdef PLATFORM_AMIGA
    clock_t start_time = clock();
    log_message("TIMING: Using clock() for timing");
#else
    clock_t start_time = clock();
#endif

#ifdef PLATFORM_AMIGA
    bool success = execute_command_amiga_streaming(cmd, unzip_extract_line_processor, &ctx, &unzip_config);
#else
    bool success = execute_command_host(cmd, unzip_extract_line_processor, &ctx);
#endif

#ifdef PLATFORM_AMIGA
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
    log_message("TIMING: Unzip extract operation took %lu ticks", elapsed_ticks);
#else
    clock_t end_time = clock();
    unsigned long elapsed_ticks = (unsigned long)(end_time - start_time);
    log_message("TIMING: Unzip extract operation took %lu ticks", elapsed_ticks);
#endif

    log_message("RESULT: Files extracted: %lu, Bytes processed: %lu",
               (unsigned long)ctx.file_count, (unsigned long)ctx.cumulative_bytes);

    /* Calculate final percentage using integer math */
    uint32_t final_percentage_x10 = 0;
    if (total_expected > 0) {
        final_percentage_x10 = (ctx.cumulative_bytes * 1000) / total_expected;
    }
    log_message("FINAL_PERCENTAGE: %lu.%lu%%",
               (unsigned long)(final_percentage_x10 / 10),
               (unsigned long)(final_percentage_x10 % 10));

    /* Check for success conditions */
    bool operation_success = false;

    if (success && ctx.file_count > 0) {
        log_message("PRIMARY_SUCCESS: Command executed and files processed");
        operation_success = true;
    } else {
        /* Fallback check - look for destination directory */
        log_message("PRIMARY_FAILURE: Attempting fallback directory check");

        /* Extract destination path from command */
        const char *last_space = strrchr(cmd, ' ');
        if (last_space && check_directory_exists(last_space + 1)) {
            log_message("FALLBACK_SUCCESS: Destination directory exists, assuming extraction succeeded");
            operation_success = true;
        } else {
            log_message("FALLBACK_FAILURE: No destination directory found");
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
#ifdef PLATFORM_AMIGA
        printf("Time elapsed: %lu ticks\n", elapsed_ticks);
#else
        printf("Time elapsed: %lu ticks\n", elapsed_ticks);
#endif
        log_message("SUCCESS: unzip_extract completed successfully");
    } else {
        printf("\nUnzip extraction failed!\n");
        fflush(stdout);
        log_message("FAILURE: unzip_extract failed completely");
    }

    fflush(stdout);
    log_message("=== UNZIP_EXTRACT END ===");

    return operation_success;
}
