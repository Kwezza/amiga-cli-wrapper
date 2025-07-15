#include "lha_wrapper.h"
#include "process_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Internal helper functions */
static void lha_log_message(const char *format, ...);
static bool lha_list_line_processor(const char *line, void *user_data);
static bool lha_extract_line_processor(const char *line, void *user_data);
static bool parse_lha_list_line(const char *line, uint32_t *file_size);
static bool parse_lha_extract_line(const char *line, uint32_t *file_size, char *filename, size_t filename_max);
static void strip_escape_codes(const char *input, char *output, size_t output_size);

/* Data structures for line processing callbacks */
typedef struct {
    uint32_t total_size;
    uint32_t file_count;
    bool completion_detected;
} lha_list_context_t;

typedef struct {
    uint32_t total_expected;
    uint32_t cumulative_bytes;
    uint32_t file_count;
    uint32_t last_percentage_x10;
    bool completion_detected;
} lha_extract_context_t;

/* Global state */
static FILE *g_lha_logfile = NULL;
static bool g_lha_initialized = false;

bool lha_wrapper_init(void)
{
    if (g_lha_initialized) {
        return true;
    }

    /* Initialize process control system */
    if (!process_control_init()) {
        return false;
    }

    /* Try to open LHA log file */
    g_lha_logfile = fopen("logfile.txt", "a");
    if (!g_lha_logfile) {
#ifdef PLATFORM_AMIGA
        g_lha_logfile = fopen("T:logfile.txt", "a");
        if (!g_lha_logfile) {
            g_lha_logfile = fopen("RAM:logfile.txt", "a");
        }
#endif
    }

    g_lha_initialized = true;

    if (g_lha_logfile) {
        lha_log_message("=== LHA Wrapper System Initialized ===");
    }

    return true;
}

void lha_wrapper_cleanup(void)
{
    if (g_lha_logfile) {
        lha_log_message("=== LHA Wrapper System Cleanup ===");
        fclose(g_lha_logfile);
        g_lha_logfile = NULL;
    }

    process_control_cleanup();
    g_lha_initialized = false;
}

bool lha_controlled_list(const char *cmd, uint32_t *out_total, uint32_t *out_file_count)
{
    if (!cmd || !out_total) {
        return false;
    }

    if (!g_lha_initialized) {
        if (!lha_wrapper_init()) {
            return false;
        }
    }

    *out_total = 0;
    if (out_file_count) {
        *out_file_count = 0;
    }

    lha_log_message("Starting LHA controlled list operation");
    lha_log_message("Command: %s", cmd);

    /* Set up list context */
    lha_list_context_t ctx = {0, 0, false};

    /* Configure process execution */
    process_exec_config_t config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_list",
        .timeout_seconds = 30,
        .silent_mode = false
    };

    /* Execute controlled process */
    controlled_process_t process;
    bool result = execute_controlled_process(cmd, lha_list_line_processor, &ctx, &config, &process);

    if (result) {
        *out_total = ctx.total_size;
        if (out_file_count) {
            *out_file_count = ctx.file_count;
        }
        
        /* Check for exit code */
        int32_t exit_code;
        if (get_process_exit_code(&process, &exit_code)) {
            lha_log_message("LHA list exit code: %ld", (long)exit_code);
            if (exit_code != 0) {
                lha_log_message("Warning: LHA list returned non-zero exit code: %ld", (long)exit_code);
            }
        }
        
        lha_log_message("LHA list completed successfully");
        lha_log_message("Total files: %lu", (unsigned long)ctx.file_count);
        lha_log_message("Total size: %lu bytes", (unsigned long)ctx.total_size);
    } else {
        lha_log_message("LHA list failed");
    }

    /* Clean up process resources */
    cleanup_controlled_process(&process);

    return result;
}

bool lha_controlled_extract(const char *cmd, uint32_t total_expected)
{
    if (!cmd) {
        return false;
    }

    if (!g_lha_initialized) {
        if (!lha_wrapper_init()) {
            return false;
        }
    }

    lha_log_message("Starting LHA controlled extract operation");
    lha_log_message("Command: %s", cmd);
    lha_log_message("Expected total: %lu bytes", (unsigned long)total_expected);

    /* Set up extract context */
    lha_extract_context_t ctx = {
        .total_expected = total_expected,
        .cumulative_bytes = 0,
        .file_count = 0,
        .last_percentage_x10 = 0,
        .completion_detected = false
    };

    /* Configure process execution */
    process_exec_config_t config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_extract",
        .timeout_seconds = 60,
        .silent_mode = false
    };

    /* Execute controlled process */
    controlled_process_t process;
    bool result = execute_controlled_process(cmd, lha_extract_line_processor, &ctx, &config, &process);

    if (result) {
        /* Check for exit code */
        int32_t exit_code;
        if (get_process_exit_code(&process, &exit_code)) {
            lha_log_message("LHA extract exit code: %ld", (long)exit_code);
            if (exit_code != 0) {
                lha_log_message("Warning: LHA extract returned non-zero exit code: %ld", (long)exit_code);
                lha_log_message("This usually indicates file creation errors or warnings");
            }
        }
        
        lha_log_message("LHA extract completed successfully");
        lha_log_message("Files extracted: %lu", (unsigned long)ctx.file_count);
        lha_log_message("Bytes extracted: %lu", (unsigned long)ctx.cumulative_bytes);
    } else {
        lha_log_message("LHA extract failed");
    }

    /* Clean up process resources */
    cleanup_controlled_process(&process);

    return result;
}

/* Internal helper functions */

static void lha_log_message(const char *format, ...)
{
    if (!g_lha_logfile) {
        return;
    }

    /* Add timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    fprintf(g_lha_logfile, "[%02d:%02d:%02d] LHA: ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    va_list args;
    va_start(args, format);
    vfprintf(g_lha_logfile, format, args);
    va_end(args);

    fprintf(g_lha_logfile, "\n");
    fflush(g_lha_logfile);
}

static bool lha_list_line_processor(const char *line, void *user_data)
{
    lha_list_context_t *ctx = (lha_list_context_t *)user_data;
    
    if (!line || !ctx) {
        return false;
    }

    /* Strip escape codes from line */
    char clean_line[256];
    strip_escape_codes(line, clean_line, sizeof(clean_line));

    lha_log_message("Processing list line: %s", clean_line);

    /* Check for completion indicators */
    if (strstr(clean_line, "Operation successful") || 
        strstr(clean_line, "files") || 
        strstr(clean_line, "----")) {
        
        if (strstr(clean_line, "Operation successful")) {
            ctx->completion_detected = true;
            lha_log_message("LHA operation completion detected");
        }
        return true;
    }

    /* Parse file information */
    uint32_t file_size;
    if (parse_lha_list_line(clean_line, &file_size)) {
        ctx->total_size += file_size;
        ctx->file_count++;
        
        lha_log_message("Parsed file: size=%lu, total=%lu", 
                       (unsigned long)file_size, (unsigned long)ctx->total_size);
    }

    return true;
}

static bool lha_extract_line_processor(const char *line, void *user_data)
{
    lha_extract_context_t *ctx = (lha_extract_context_t *)user_data;
    
    if (!line || !ctx) {
        return false;
    }

    /* Strip escape codes from line */
    char clean_line[256];
    strip_escape_codes(line, clean_line, sizeof(clean_line));

    lha_log_message("Processing extract line: %s", clean_line);

    /* Check for completion indicators */
    if (strstr(clean_line, "Operation successful")) {
        ctx->completion_detected = true;
        lha_log_message("LHA extraction completion detected");
        return true;
    }

    /* Parse extraction information */
    uint32_t file_size;
    char filename[64];
    
    if (parse_lha_extract_line(clean_line, &file_size, filename, sizeof(filename))) {
        ctx->cumulative_bytes += file_size;
        ctx->file_count++;
        
        /* Calculate percentage (x10 to avoid floating point) */
        uint32_t percentage_x10 = 0;
        if (ctx->total_expected > 0) {
            percentage_x10 = (ctx->cumulative_bytes * 1000) / ctx->total_expected;
        }
        
        /* Log progress at 1% intervals */
        if (percentage_x10 > ctx->last_percentage_x10 + 10) {
            lha_log_message("Progress: %lu.%lu%% (%lu/%lu bytes)", 
                           (unsigned long)(percentage_x10 / 10), 
                           (unsigned long)(percentage_x10 % 10),
                           (unsigned long)ctx->cumulative_bytes,
                           (unsigned long)ctx->total_expected);
            ctx->last_percentage_x10 = percentage_x10;
        }
        
        lha_log_message("Extracted: %s (%lu bytes)", filename, (unsigned long)file_size);
    }

    return true;
}

static bool parse_lha_list_line(const char *line, uint32_t *file_size)
{
    if (!line || !file_size) {
        return false;
    }

    /* Expected format: "   10380    6306 39.2% 06-Jul-112 19:06:46 +A10" */
    /* Look for lines starting with whitespace followed by numbers */
    
    char *endptr;
    const char *start = line;
    
    /* Skip leading whitespace */
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    
    /* Try to parse the first number (original size) */
    unsigned long size = strtoul(start, &endptr, 10);
    
    /* Check if we got a valid number and the line continues appropriately */
    if (endptr != start && size > 0 && (*endptr == ' ' || *endptr == '\t')) {
        *file_size = (uint32_t)size;
        return true;
    }
    
    return false;
}

static bool parse_lha_extract_line(const char *line, uint32_t *file_size, char *filename, size_t filename_max)
{
    if (!line || !file_size || !filename) {
        return false;
    }

    /* Expected format: " Extracting: (   10380)  A10TankKiller3Disk/data/A10" */
    
    const char *extracting_pos = strstr(line, "Extracting:");
    if (!extracting_pos) {
        return false;
    }
    
    /* Look for the opening parenthesis */
    const char *paren_pos = strchr(extracting_pos, '(');
    if (!paren_pos) {
        return false;
    }
    
    /* Parse the size */
    char *endptr;
    unsigned long size = strtoul(paren_pos + 1, &endptr, 10);
    
    if (endptr == paren_pos + 1 || *endptr != ')') {
        return false;
    }
    
    /* Find the filename after the closing parenthesis */
    const char *filename_start = strchr(endptr, ')');
    if (!filename_start) {
        return false;
    }
    
    filename_start++; /* Skip the ')' */
    
    /* Skip whitespace */
    while (*filename_start == ' ' || *filename_start == '\t') {
        filename_start++;
    }
    
    /* Copy filename */
    size_t copy_len = 0;
    while (filename_start[copy_len] != '\0' && copy_len < filename_max - 1) {
        filename[copy_len] = filename_start[copy_len];
        copy_len++;
    }
    filename[copy_len] = '\0';
    
    /* Remove trailing whitespace */
    size_t len = strlen(filename);
    while (len > 0 && (filename[len-1] == ' ' || filename[len-1] == '\t' || 
                       filename[len-1] == '\n' || filename[len-1] == '\r')) {
        filename[--len] = '\0';
    }
    
    *file_size = (uint32_t)size;
    return true;
}

static void strip_escape_codes(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        return;
    }

    size_t out_pos = 0;
    size_t in_pos = 0;
    size_t input_len = strlen(input);

    while (in_pos < input_len && out_pos < output_size - 1) {
        if (input[in_pos] == '\x1b' || input[in_pos] == '[') {
            /* Skip escape sequences */
            while (in_pos < input_len && 
                   (input[in_pos] < 'A' || input[in_pos] > 'Z') &&
                   (input[in_pos] < 'a' || input[in_pos] > 'z')) {
                in_pos++;
            }
            if (in_pos < input_len) {
                in_pos++; /* Skip the final character */
            }
        } else {
            output[out_pos++] = input[in_pos++];
        }
    }

    output[out_pos] = '\0';
}
