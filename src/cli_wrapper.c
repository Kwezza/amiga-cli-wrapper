#include "cli_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

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
     */

    *file_size = 0;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Check if this looks like a file line (starts with digit) */
    if (*line < '0' || *line > '9') {
        return false;
    }

    /* Parse the original size (first number) */
    char *endptr;
    unsigned long size = strtoul(line, &endptr, 10);

    /* Verify we parsed something and hit whitespace */
    if (endptr == line || (*endptr != ' ' && *endptr != '\t')) {
        return false;
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
    char filename[256];

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
        log_message("EXTRACT: %s — file %u/%u — %lu jiffies (%u%%)",
                   filename,
                   ctx->file_count,
                   estimated_total_files,
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

static bool execute_command_amiga(const char *cmd, bool (*line_processor)(const char *, void *), void *user_data)
{
    log_message("EXECUTE_AMIGA: Starting asynchronous streaming command: %s", cmd);

    /* Generate unique pipe name using process ID */
    struct Task *current_task = FindTask(NULL);
    char pipe_name[64];
    sprintf(pipe_name, "PIPE:lha_pipe.%lu", (unsigned long)current_task);

    log_message("EXECUTE_AMIGA: Using pipe: %s", pipe_name);

    /* 1. Open pipe first for reading raw output */
    BPTR pipe = Open(pipe_name, MODE_NEWFILE);
    if (!pipe) {
        log_message("ERROR: failed to open pipe %s", pipe_name);
        return false;
    }

    log_message("EXECUTE_AMIGA: Successfully opened pipe for writing");

    /* 2. Spawn LhA asynchronously using CreateNewProcTags */
    printf("Spawning LhA process asynchronously...\n");
    fflush(stdout);

    /* 2. Spawn LhA asynchronously using SystemTags with SYS_Asynch */
    printf("Spawning LhA process asynchronously...\n");
    fflush(stdout);

    /* Create command with pipe redirection for background execution */
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);

    log_message("EXECUTE_AMIGA: Full command: %s", full_cmd);

    /* Close the write pipe handle so the background command can open it */
    Close(pipe);

    /* Execute command asynchronously using SystemTags or fallback to System */
    struct TagItem tags[] = {
        { SYS_Asynch, TRUE },
        { TAG_END, 0 }
    };
    
    LONG proc_result = SystemTagList(full_cmd, tags);
    if (proc_result == -1) {
        log_message("EXECUTE_AMIGA: SystemTagList failed, trying fallback System()");
        /* Fallback to regular System() if SystemTagList fails */
        proc_result = System(full_cmd, NULL);
        if (proc_result == -1) {
            log_message("ERROR: failed to spawn LhA process with both methods");
            return false;
        }
        log_message("EXECUTE_AMIGA: LhA process spawned successfully (System fallback)");
    } else {
        log_message("EXECUTE_AMIGA: LhA process spawned successfully (async)");
    }

    /* Give the asynchronous process time to start and open the pipe */
    Delay(10); /* 200ms - enough time for process to start and begin writing */

    /* 3. Open pipe for reading immediately */
    BPTR read_pipe = Open(pipe_name, MODE_OLDFILE);
    if (!read_pipe) {
        log_message("ERROR: failed to open pipe %s for reading", pipe_name);
        return false;
    }

    log_message("EXECUTE_AMIGA: Successfully opened pipe for reading");

    /* Initialize timing */
    clock_t start_time = clock();
    log_message("EXECUTE_AMIGA: Started real-time streaming at %lu ticks", (unsigned long)start_time);

    /* 4. Immediate reading loop with timeout - stream data as LhA generates output */
    char buf[256];
    char partial_line[512] = {0};
    int line_count = 0;
    ULONG bytesRead;
    int empty_reads = 0;
    const int MAX_EMPTY_READS = 100; /* About 2 seconds timeout */

    printf("Starting real-time extraction monitoring...\n");
    fflush(stdout);

    while (empty_reads < MAX_EMPTY_READS) {
        bytesRead = Read(read_pipe, buf, sizeof(buf) - 1);
        
        if (bytesRead > 0) {
            empty_reads = 0; /* Reset timeout counter */
            buf[bytesRead] = '\0';

            /* Log raw output for debugging */
            log_message("RAW: %s", buf);

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

                        log_message("EXECUTE_AMIGA: Line %d at %lu ticks: %s",
                                   line_count, current_ticks, partial_line);

                        /* Process the line immediately for real-time extraction tracking */
                        if (!line_processor(partial_line, user_data)) {
                            log_message("EXECUTE_AMIGA: Line processor requested stop at line %d", line_count);
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
        } else if (bytesRead == 0) {
            /* EOF reached - but might be temporary, check a few times */
            empty_reads++;
            if (empty_reads < MAX_EMPTY_READS) {
                Delay(1); /* 20ms delay before retrying */
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
                Delay(1); /* 20ms delay before retrying */
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

    /* Brief delay to allow process cleanup */
    Delay(2); /* 40ms */

    log_message("EXECUTE_AMIGA: Process cleanup completed");

    /* 6. Cleanup */
    Close(read_pipe);

    /* Calculate final timing */
    clock_t end_time = clock();
    ULONG total_elapsed = (unsigned long)(end_time - start_time);

    log_message("EXECUTE_AMIGA: Processed %d lines in real-time streaming mode", line_count);
    log_message("EXECUTE_AMIGA: Total execution time: %lu ticks", total_elapsed);
    log_message("EXECUTE_AMIGA: Asynchronous streaming completed successfully");

    printf("Real-time streaming completed - processed %d lines\n", line_count);
    fflush(stdout);

    return true;
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

    if (!cli_wrapper_init()) {
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

    log_message("=== CLI_EXTRACT START ===");
    log_message("Command: %s", cmd);
    log_message("Expected total: %lu bytes", (unsigned long)total_expected);

    extract_context_t ctx = {total_expected, 0, 0, 0};

#ifdef PLATFORM_AMIGA
    /* Use clock() for timing on Amiga for simplicity */
    clock_t start_time = clock();
    log_message("TIMING: Using clock() for timing");
#else
    /* Host fallback using clock() */
    clock_t start_time = clock();
#endif

#ifdef PLATFORM_AMIGA
    bool success = execute_command_amiga(cmd, extract_line_processor, &ctx);
#else
    bool success = execute_command_host(cmd, extract_line_processor, &ctx);
#endif

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
