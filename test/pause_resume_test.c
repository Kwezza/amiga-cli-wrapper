#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>

#ifdef PLATFORM_AMIGA
#include <dos/dos.h>
#include <proto/dos.h>
#endif

/* Include the process control system */
#include "../src/process_control.h"

/* Test configuration */
#define TEST_ARCHIVE "assets/A10TankKiller_v2.0_3Disk.lha"
#define TEST_DEST_DIR "temp_extract/"

/* Test result tracking */
static FILE *test_logfile = NULL;

/* Enhanced progress tracking context for pause-then-quit test */
typedef struct {
    uint32_t total_files;
    uint32_t processed_files;
    uint32_t files_at_pause;
    bool pause_requested;
    bool quit_requested;
    bool completion_detected;
    bool output_after_quit;
    uint32_t lines_after_quit;
    controlled_process_t *process;
} pause_resume_context_t;

/* Test helper functions */
static void test_log(const char *format, ...);
static bool create_directory(const char *path);
static bool check_directory_exists(const char *path);

/* Helper function to parse LHA extract lines */
static bool parse_test_extract_line(const char *line, char *filename, size_t filename_max);

/* Helper function to strip escape codes */
static void strip_test_escape_codes(const char *input, char *output, size_t output_size);

/* Line processor for pause/resume test */
static bool pause_resume_line_processor(const char *line, void *user_data);

/* Request 64KB stack for Amiga build */
size_t __stack = 65536;  /* request a 64 KB stack */

int main(void)
{
    printf("=== Amiga Pause/Resume Test (Standalone) ===\n");
    printf("Platform: %s\n",
#ifdef PLATFORM_AMIGA
        "Amiga (7MHz - Real Process Control Test)"
#else
        "Host (stubbed)"
#endif
    );

    /* Open test log file */
    test_logfile = fopen("pause_resume_log.txt", "w");
    if (!test_logfile) {
        printf("Warning: Could not create test logfile\n");
    }

    test_log("=== Pause/Resume Test Started ===");

    /* Initialize process control system */
    if (!process_control_init()) {
        printf("ERROR: Failed to initialize process control system\n");
        test_log("ERROR: Failed to initialize process control system");
        return 1;
    }

    printf("Process control system initialized successfully\n");
    test_log("Process control system initialized successfully");

    /* Create destination directory */
    if (!create_directory(TEST_DEST_DIR)) {
        printf("Warning: Could not create destination directory: %s\n", TEST_DEST_DIR);
        test_log("Warning: Could not create destination directory: %s", TEST_DEST_DIR);
    }

    /* Set up pause-then-quit context */
    pause_resume_context_t ctx = {
        .total_files = 38,  /* Known file count for this archive */
        .processed_files = 0,
        .files_at_pause = 0,
        .pause_requested = false,
        .quit_requested = false,
        .completion_detected = false,
        .output_after_quit = false,
        .lines_after_quit = 0,
        .process = NULL
    };

    printf("\nStarting LHA extraction with real pause/resume control...\n");
    test_log("Starting LHA extraction with real pause/resume control");

    /* Build extraction command */
    char extract_cmd[512];
    snprintf(extract_cmd, sizeof(extract_cmd), "lha x -m -n %s %s", TEST_ARCHIVE, TEST_DEST_DIR);

    test_log("LHA extraction command: %s", extract_cmd);
    printf("Command: %s\n", extract_cmd);

    /* Configure process execution */
    process_exec_config_t config = {
        .tool_name = "LhA",
        .pipe_prefix = "pause_test",
        .timeout_seconds = 120,
        .silent_mode = false
    };

    /* Start controlled process */
    controlled_process_t process;
    ctx.process = &process;

    printf("\nStarting controlled LHA process...\n");
    test_log("Starting controlled LHA process");

    /* Execute the process with our pause/resume line processor */
    bool result = execute_controlled_process(extract_cmd, pause_resume_line_processor, &ctx, &config, &process);

    test_log("LHA extraction result: %s", result ? "success" : "failure");
    printf("\nLHA extraction result: %s\n", result ? "success" : "failure");

    /* Report results */
    printf("\n=== Pause-Then-Quit Test Results ===\n");
    printf("Files processed: %lu\n", (unsigned long)ctx.processed_files);
    printf("Files at pause point: %lu\n", (unsigned long)ctx.files_at_pause);
    printf("Pause requested: %s\n", ctx.pause_requested ? "yes" : "no");
    printf("Quit requested: %s\n", ctx.quit_requested ? "yes" : "no");
    printf("Output after quit: %s\n", ctx.output_after_quit ? "yes" : "no");
    printf("Lines after quit: %lu\n", (unsigned long)ctx.lines_after_quit);
    printf("Completion detected: %s\n", ctx.completion_detected ? "yes" : "no");

    test_log("Final results: processed=%lu, pause_point=%lu, pause_req=%s, quit_req=%s, output_after_quit=%s, lines_after_quit=%lu, complete=%s",
             (unsigned long)ctx.processed_files, (unsigned long)ctx.files_at_pause,
             ctx.pause_requested ? "yes" : "no",
             ctx.quit_requested ? "yes" : "no",
             ctx.output_after_quit ? "yes" : "no",
             (unsigned long)ctx.lines_after_quit,
             ctx.completion_detected ? "yes" : "no");

    /* Clean up */
    cleanup_controlled_process(&process);
    process_control_cleanup();

    if (test_logfile) {
        fclose(test_logfile);
    }

    printf("\nTest completed. Check 'pause_resume_log.txt' for detailed log.\n");
    
    return 0;
}

static void test_log(const char *format, ...)
{
    if (!test_logfile) {
        return;
    }

    /* Add timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    fprintf(test_logfile, "[%02d:%02d:%02d] ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    va_list args;
    va_start(args, format);
    vfprintf(test_logfile, format, args);
    va_end(args);

    fprintf(test_logfile, "\n");
    fflush(test_logfile);
}

static bool check_directory_exists(const char *path)
{
    if (!path) {
        return false;
    }

#ifdef PLATFORM_AMIGA
    BPTR lock = Lock(path, ACCESS_READ);
    if (lock) {
        UnLock(lock);
        return true;
    }
    return false;
#else
    /* Host implementation */
    FILE *test = fopen(path, "r");
    if (test) {
        fclose(test);
        return true;
    }
    return false;
#endif
}

static bool create_directory(const char *path)
{
    if (!path) {
        return false;
    }

    test_log("Creating directory: %s", path);

    if (check_directory_exists(path)) {
        test_log("Directory already exists: %s", path);
        return true;
    }

#ifdef PLATFORM_AMIGA
    BPTR lock = CreateDir(path);
    if (lock) {
        UnLock(lock);
        test_log("Directory created successfully: %s", path);
        return true;
    }
    test_log("Failed to create directory: %s", path);
    return false;
#else
    /* Host implementation */
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", path);
    int result = system(mkdir_cmd);
    
    if (result == 0) {
        test_log("Directory created successfully: %s", path);
        return true;
    }
    
    test_log("Failed to create directory: %s", path);
    return false;
#endif
}

static bool parse_test_extract_line(const char *line, char *filename, size_t filename_max)
{
    if (!line || !filename) {
        return false;
    }

    /* Expected format: " Extracting: (   10380)  A10TankKiller3Disk/data/A10" */
    
    const char *extracting_pos = strstr(line, "Extracting:");
    if (!extracting_pos) {
        return false;
    }
    
    /* Look for the closing parenthesis */
    const char *paren_pos = strchr(extracting_pos, ')');
    if (!paren_pos) {
        return false;
    }
    
    /* Find the filename after the closing parenthesis */
    const char *filename_start = paren_pos + 1;
    
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
    
    return (len > 0);
}

static void strip_test_escape_codes(const char *input, char *output, size_t output_size)
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

static bool pause_resume_line_processor(const char *line, void *user_data)
{
    pause_resume_context_t *ctx = (pause_resume_context_t *)user_data;
    
    if (!line || !ctx) {
        return true;
    }

    /* Strip escape codes from line */
    char clean_line[256];
    strip_test_escape_codes(line, clean_line, sizeof(clean_line));

    test_log("Processing line: %s", clean_line);
    
    /* Check if we received output after quit signal was sent */
    if (ctx->pause_requested && ctx->quit_requested) {
        ctx->output_after_quit = true;
        ctx->lines_after_quit++;
        test_log("OUTPUT AFTER QUIT SIGNAL: %s", clean_line);
        printf("   [POST-QUIT] %s\n", clean_line);
    }

    /* Parse extraction information */
    char filename[128];
    
    if (parse_test_extract_line(clean_line, filename, sizeof(filename))) {
        ctx->processed_files++;
        
        /* Calculate percentage */
        uint32_t percentage = 0;
        if (ctx->total_files > 0) {
            percentage = (ctx->processed_files * 100) / ctx->total_files;
        }
        
        if (!ctx->quit_requested) {
            printf("   [%3lu%%] %s (%lu/%lu files)\n", 
                   (unsigned long)percentage,
                   filename,
                   (unsigned long)ctx->processed_files,
                   (unsigned long)ctx->total_files);
        }
        
        test_log("Extracted file [%lu%%]: %s (%lu/%lu)", 
                 (unsigned long)percentage, filename,
                 (unsigned long)ctx->processed_files, (unsigned long)ctx->total_files);

        /* REAL PAUSE-THEN-QUIT TESTING: Pause after 5 files, then quit */
        if (ctx->processed_files == 5 && !ctx->pause_requested) {
            ctx->pause_requested = true;
            ctx->files_at_pause = ctx->processed_files;
            
            printf("\n*** PAUSE-THEN-QUIT TEST AFTER 5 FILES ***\n");
            test_log("PAUSE-THEN-QUIT TEST AFTER 5 FILES");
            
            /* Add debugging for the process state */
            printf("Process running: %s\n", ctx->process->process_running ? "YES" : "NO");
            printf("Child process: %p\n", (void *)ctx->process->child_process);
            test_log("Process running: %s", ctx->process->process_running ? "YES" : "NO");
            test_log("Child process: %p", ctx->process->child_process);
            
            /* Step 1: Send pause signal to stop text updates */
            printf("Step 1: Pausing output (simulating user prompt)...\n");
            test_log("Step 1: Pausing output (simulating user prompt)");
            
            if (ctx->process && send_pause_signal(ctx->process)) {
                printf("Pause signal sent successfully - output should stop\n");
                test_log("Pause signal sent successfully - output should stop");
            } else {
                printf("Failed to send pause signal\n");
                test_log("Failed to send pause signal");
            }
            
            printf("Simulating user cancel prompt (3 seconds)...\n");
            test_log("Simulating user cancel prompt (3 seconds)");
            
            /* Wait 3 seconds to simulate user decision time */
            volatile int delay_counter;
            int seconds_remaining = 3;
            
            while (seconds_remaining > 0) {
                /* Wait approximately 1 second */
                for (delay_counter = 0; delay_counter < 500000; delay_counter++) {
                    /* 1 second equivalent busy-wait */
                }
                
                printf("User prompt: Cancel process? (Y/n) - %d seconds...\n", seconds_remaining);
                test_log("User prompt simulation: %d seconds remaining", seconds_remaining);
                seconds_remaining--;
            }
            
            /* Step 2: Send quit command (CTRL+C) to terminate process */
            printf("\nStep 2: User chose to cancel - sending quit command...\n");
            test_log("Step 2: User chose to cancel - sending quit command");
            
            if (ctx->process && send_terminate_signal(ctx->process)) {
                printf("Quit signal (CTRL+C) sent successfully to LHA process\n");
                test_log("Quit signal (CTRL+C) sent successfully to LHA process");
                ctx->quit_requested = true;
            } else {
                printf("Failed to send quit signal to LHA process\n");
                test_log("Failed to send quit signal to LHA process");
            }
            
            printf("Step 3: Monitoring for process termination...\n");
            test_log("Step 3: Monitoring for process termination");
            
            /* Mark completion as detected early since we're terminating */
            ctx->completion_detected = true;
            
            printf("*** PROCESS TERMINATION REQUESTED ***\n\n");
            test_log("Process termination requested - monitoring for final output");
        }
    }

    /* Check for completion */
    if (strstr(clean_line, "files extracted") && strstr(clean_line, "all files OK")) {
        ctx->completion_detected = true;
        test_log("LHA extraction completion detected");
        if (!ctx->quit_requested) {
            printf("\nExtraction completed successfully!\n");
        } else {
            printf("\nExtraction completed after quit signal!\n");
            test_log("WARNING: Extraction completed despite quit signal");
        }
    }

    return true;
}
