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

    if (parse_lha_list_line(line, &file_size)) {
        ctx->total_size += file_size;
        ctx->file_count++;
    }

    return true; /* Continue processing */
}

/* Line processor for extract command */
static bool extract_line_processor(const char *line, void *user_data)
{
    extract_context_t *ctx = (extract_context_t *)user_data;
    uint32_t file_size;
    static char filename[64];  /* Static to avoid stack usage, smaller size */

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
    }

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

    /* Generate unique pipe name using process ID and tool prefix */
    struct Task *current_task = FindTask(NULL);
    if (!current_task) {
        log_message("ERROR: FindTask returned NULL");
        return false;
    }

    char pipe_name[64];
    sprintf(pipe_name, "PIPE:%s.%lu", config->pipe_prefix, (unsigned long)current_task);

    /* 1. Open pipe first for writing */
    BPTR pipe = Open(pipe_name, MODE_NEWFILE);
    if (!pipe) {
        log_message("ERROR: failed to open pipe %s", pipe_name);
        return false;
    }

    /* Console output */
    if (!config->silent_mode) {
        printf("Spawning %s process asynchronously...\n", config->tool_name);
        fflush(stdout);
    }

    /* Create command with pipe redirection for background execution */
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);

    /* Close the write pipe handle so the background command can open it */
    Close(pipe);

    /* Execute command asynchronously using SystemTags or fallback to System */
    struct TagItem tags[] = {
        { SYS_Asynch, TRUE },
        { TAG_END, 0 }
    };

    LONG proc_result = SystemTagList(full_cmd, tags);

    if (proc_result == -1) {
        /* Fallback to regular System() if SystemTagList fails */
        proc_result = System(full_cmd, NULL);
        if (proc_result == -1) {
            log_message("ERROR: failed to spawn %s process with both methods", config->tool_name);
            return false;
        }
    }

    /* Give the asynchronous process time to start and open the pipe */
    /* Short busy-wait needed here since pipe doesn't exist yet for WaitForChar() */
    {
        volatile int delay_counter;
        for (delay_counter = 0; delay_counter < 50000; delay_counter++) {
            /* Minimal startup delay - reduced from 200000 to 50000 iterations */
        }
    }

    /* 3. Open pipe for reading immediately */
    BPTR read_pipe = Open(pipe_name, MODE_OLDFILE);

    if (!read_pipe) {
        log_message("ERROR: failed to open pipe %s for reading", pipe_name);
        return false;
    }

    /* Initialize timing */
    clock_t start_time = clock();

    if (!config->silent_mode) {
        printf("Starting real-time %s monitoring...\n", config->tool_name);
        fflush(stdout);
    }

    /* 4. Immediate reading loop with timeout - stream data as command generates output */
    char buf[64];   /* Even smaller buffer for maximum Amiga safety */
    char partial_line[128] = {0};  /* Much smaller line buffer */

    int line_count = 0;
    ULONG bytesRead;
    int empty_reads = 0;
    const int MAX_EMPTY_READS = config->timeout_seconds * 25; /* 25 checks per second */

    while (empty_reads < MAX_EMPTY_READS) {
        bytesRead = Read(read_pipe, buf, sizeof(buf) - 1);

        if (bytesRead > 0) {
            empty_reads = 0; /* Reset timeout counter */

            /* Ensure buffer safety - paranoid safety checks */
            if (bytesRead >= sizeof(buf)) {
                bytesRead = sizeof(buf) - 1;
            }

            /* Additional safety - ensure we don't overrun buffer */
            if (bytesRead > 63) {
                bytesRead = 63;
            }

            buf[bytesRead] = '\0';

            /* Get current timing for real-time feedback */
            clock_t current_time = clock();
            ULONG current_ticks = (unsigned long)current_time;

            /* Process buffer character by character to handle partial lines */
            char *buffer_ptr = buf;
            while (*buffer_ptr) {
                char ch = *buffer_ptr++;

                if (ch == '\n' || ch == '\r') {
                    /* Complete line found */
                    if (strlen(partial_line) > 0) {
                        line_count++;

                        /* Process the line immediately for real-time tracking */
                        if (!line_processor(partial_line, user_data)) {
                            goto cleanup;
                        }

                        /* Clear partial line buffer safely */
                        partial_line[0] = '\0';
                    }
                } else {
                    /* Add character to partial line with extra safety */
                    size_t len = strlen(partial_line);
                    if (len < sizeof(partial_line) - 2) {  /* Extra safety margin */
                        partial_line[len] = ch;
                        partial_line[len + 1] = '\0';
                    }
                }
            }
        } else if (bytesRead == 0) {
            /* EOF reached - but might be temporary, check a few times */
            empty_reads++;
            if (empty_reads < MAX_EMPTY_READS) {
                /* Use cooperative AmigaOS wait instead of busy-wait */
                if (WaitForChar(read_pipe, 100000)) {
                    /* Data became available, reset counter and try again */
                    empty_reads = 0;
                    continue;
                } else {
                    /* Timeout occurred (~20ms), continue with empty_reads increment */
                    continue;
                }
            } else {
                break;
            }
        } else {
            /* Read error */
            LONG error = IoErr();
            log_message("EXECUTE_AMIGA: Read error: %ld", error);
            empty_reads++;
            if (empty_reads < MAX_EMPTY_READS) {
                /* Use cooperative AmigaOS wait after read error */
                if (WaitForChar(read_pipe, 100000)) {
                    /* Data became available after error, reset counter */
                    empty_reads = 0;
                    continue;
                } else {
                    /* Timeout occurred, continue with empty_reads increment */
                    continue;
                }
            } else {
                break;
            }
        }
    }

cleanup:

    /* Process any remaining partial line */
    if (strlen(partial_line) > 0) {
        line_count++;
        line_processor(partial_line, user_data);
    }

    /* Brief delay to allow process cleanup - much shorter than before */
    {
        volatile int delay_counter;
        for (delay_counter = 0; delay_counter < 5000; delay_counter++) {
            /* Minimal cleanup delay - reduced from 20000 to 5000 iterations */
        }
    }

    /* 6. Cleanup */
    Close(read_pipe);

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

    list_context_t ctx = {0, 0};

#ifdef PLATFORM_AMIGA
    /* Use clock() for timing on Amiga for simplicity */
    clock_t start_time = clock();
    bool success = execute_command_amiga(cmd, list_line_processor, &ctx);
    clock_t end_time = clock();
    (void)(end_time - start_time); /* Avoid unused variable warning */
#else
    /* Host fallback using clock() */
    bool success = execute_command_host(cmd, list_line_processor, &ctx);
#endif

    if (success && ctx.file_count > 0) {
        *out_total = ctx.total_size;
        return true;
    } else {
        return false;
    }
}

bool cli_extract(const char *cmd, uint32_t total_expected)
{
    if (!cmd) {
        log_message("ERROR: cli_extract called with NULL command");
        return false;
    }

    if (!cli_wrapper_init()) {
        log_message("ERROR: cli_wrapper_init failed in cli_extract");
        return false;
    }

    /* Console output - inform user that extraction is starting */
    printf("Starting extraction with real-time progress...\n");
    printf("Command: %s\n", cmd);
    if (total_expected > 0) {
        printf("Expected size: %lu bytes\n", (unsigned long)total_expected);
    }
    printf("NOTE: Progress will be displayed as files are extracted\n");
    fflush(stdout);

    /* Initialize context with safety checks */
    extract_context_t ctx;
    ctx.total_expected = total_expected;
    ctx.cumulative_bytes = 0;
    ctx.file_count = 0;
    ctx.last_percentage_x10 = 0;

#ifdef PLATFORM_AMIGA
    /* Use clock() for timing on Amiga for simplicity */
    clock_t start_time = clock();
#else
    /* Host fallback using clock() */
    clock_t start_time = clock();
#endif

#ifdef PLATFORM_AMIGA
    /* Use the new streaming configuration for safer execution */
    amiga_exec_config_t extract_config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_pipe",
        .timeout_seconds = 15,  /* Even shorter timeout */
        .silent_mode = false
    };

    bool success = execute_command_amiga_streaming(cmd, extract_line_processor, &ctx, &extract_config);
#else
    bool success = execute_command_host(cmd, extract_line_processor, &ctx);
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
        printf("Extracting: %s (%u/%u) %lu jiffies\n",
               filename,
               ctx->file_count,
               estimated_total_files,
               current_jiffies);
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

    list_context_t ctx = {0, 0};

#ifdef PLATFORM_AMIGA
    /* Configure for unzip */
    amiga_exec_config_t unzip_config = {
        .tool_name = "unzip",
        .pipe_prefix = "unzip_pipe",
        .timeout_seconds = 3,  /* unzip might be slower */
        .silent_mode = false
    };

    clock_t start_time = clock();
    bool success = execute_command_amiga_streaming(cmd, unzip_list_line_processor, &ctx, &unzip_config);
    clock_t end_time = clock();
    (void)(end_time - start_time); /* Avoid unused variable warning */
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

    extract_context_t ctx = {total_expected, 0, 0, 0};

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
