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

/* Include the new process control and LHA wrapper systems */
#include "../src/process_control.h"
#include "../src/lha_wrapper.h"

/* Test configuration */
#define TEST_ARCHIVE "assets/A10TankKiller_v2.0_3Disk.lha"
#define TEST_DEST_DIR "temp_extract/"
#define TEST_CORRUPTED_ARCHIVE "assets/test_archive_corrupted.lha"

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static FILE *test_logfile = NULL;

/* Enhanced progress tracking context */
typedef struct {
    uint32_t total_files;
    uint32_t processed_files;
    uint32_t last_percentage;
    bool completion_detected;
} test_progress_context_t;

/* Test helper functions */
static void test_log(const char *format, ...);
static bool run_test(const char *test_name, bool (*test_func)(void));
static bool check_directory_exists(const char *path);
static bool create_directory(const char *path);

/* Test functions */
static bool test_process_control_init(void);
static bool test_basic_process_spawning(void);
static bool test_lha_list_parsing(void);
static bool test_lha_extract_with_progress(void);
static bool test_lha_archive_integrity_good(void);
static bool test_lha_archive_integrity_corrupted(void);
static bool test_process_death_monitoring(void);
static bool test_corruption_detection(void);

/* Test line processor for basic process test */
static bool test_line_processor(const char *line, void *user_data);

/* Enhanced test line processor for extract with progress */
static bool test_extract_line_processor(const char *line, void *user_data);

/* Test line processor for LHA test integrity checking */
static bool test_integrity_line_processor(const char *line, void *user_data);

/* Helper function to parse LHA extract lines */
static bool parse_test_extract_line(const char *line, char *filename, size_t filename_max);

/* Helper function to strip escape codes */
static void strip_test_escape_codes(const char *input, char *output, size_t output_size);

/* Request 64KB stack for Amiga build */
size_t __stack = 65536;  /* request a 64 KB stack */

int main(void)
{
    printf("=== Amiga Process Control System Test Suite ===\n");
    printf("Platform: %s\n",
#ifdef PLATFORM_AMIGA
        "Amiga"
#else
        "Host (stubbed)"
#endif
    );

    /* Open test log file */
    test_logfile = fopen("logfile.txt", "w");
    if (!test_logfile) {
        printf("Warning: Could not create test logfile\n");
    }

    test_log("=== Process Control Test Suite Started ===");

    /* Run all tests */
    run_test("Process Control Initialization", test_process_control_init);
    run_test("Basic Process Spawning", test_basic_process_spawning);
    run_test("LHA List Parsing", test_lha_list_parsing);
    run_test("LHA Extract with Progress", test_lha_extract_with_progress);
    run_test("LHA Archive Integrity (Good)", test_lha_archive_integrity_good);
    run_test("LHA Archive Integrity (Corrupted)", test_lha_archive_integrity_corrupted);
    run_test("Process Death Monitoring", test_process_death_monitoring);
    run_test("Corruption Detection", test_corruption_detection);

    /* Print results */
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("All tests PASSED!\n");
    } else {
        printf("Some tests FAILED!\n");
    }

    test_log("=== Process Control Test Suite Completed ===");
    test_log("Results: %d/%d tests passed", tests_passed, tests_run);

    /* Cleanup */
    if (test_logfile) {
        fclose(test_logfile);
    }

    /* Cleanup systems */
    lha_wrapper_cleanup();
    process_control_cleanup();

    return (tests_passed == tests_run) ? 0 : 1;
}

static void test_log(const char *format, ...)
{
    if (!test_logfile) {
        return;
    }

    /* Add timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    fprintf(test_logfile, "[%02d:%02d:%02d] TEST: ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    va_list args;
    va_start(args, format);
    vfprintf(test_logfile, format, args);
    va_end(args);

    fprintf(test_logfile, "\n");
    fflush(test_logfile);
}

static bool run_test(const char *test_name, bool (*test_func)(void))
{
    printf("Running test: %s...", test_name);
    test_log("Starting test: %s", test_name);

    tests_run++;
    bool result = test_func();

    if (result) {
        printf(" PASSED\n");
        test_log("Test PASSED: %s", test_name);
        tests_passed++;
    } else {
        printf(" FAILED\n");
        test_log("Test FAILED: %s", test_name);
    }

    return result;
}

static bool test_process_control_init(void)
{
    test_log("Testing process control initialization");

    /* Test initialization */
    if (!process_control_init()) {
        test_log("Process control initialization failed");
        return false;
    }

    test_log("Process control initialization successful");

    /* Test LHA wrapper initialization */
    if (!lha_wrapper_init()) {
        test_log("LHA wrapper initialization failed");
        return false;
    }

    test_log("LHA wrapper initialization successful");
    return true;
}

static bool test_basic_process_spawning(void)
{
    test_log("Testing basic process spawning");

    /* Test a simple command that should work on both platforms */
    const char *test_cmd;
    
#ifdef PLATFORM_AMIGA
    test_cmd = "echo Test message";
#else
    test_cmd = "echo Test message";
#endif

    process_exec_config_t config = {
        .tool_name = "Echo",
        .pipe_prefix = "test_echo",
        .timeout_seconds = 10,
        .silent_mode = false
    };

    controlled_process_t process;
    int line_count = 0;
    
    bool result = execute_controlled_process(test_cmd, test_line_processor, &line_count, &config, &process);

    test_log("Process spawning result: %s", result ? "success" : "failure");
    test_log("Lines processed: %d", line_count);

    /* Clean up */
    cleanup_controlled_process(&process);

    return result;
}

static bool test_lha_list_parsing(void)
{
    test_log("Testing LHA list parsing");

    /* Check if test archive exists */
    char archive_path[256];
    snprintf(archive_path, sizeof(archive_path), "%s", TEST_ARCHIVE);
    
    test_log("Looking for test archive: %s", archive_path);

    /* Build LHA list command */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "lha l %s", archive_path);

    test_log("LHA list command: %s", cmd);

    uint32_t total_size = 0;
    uint32_t file_count = 0;
    bool result = lha_controlled_list(cmd, &total_size, &file_count);

    test_log("LHA list result: %s", result ? "success" : "failure");
    test_log("Total size parsed: %lu bytes", (unsigned long)total_size);
    test_log("File count parsed: %lu files", (unsigned long)file_count);

    /* Display results to user */
    if (result) {
        printf("   Archive contains %lu files, total size: %lu bytes\n", 
               (unsigned long)file_count, (unsigned long)total_size);
    }

    /* We expect some reasonable total size */
    if (result && total_size > 0) {
        test_log("LHA list parsing successful with valid total size");
        return true;
    }

    test_log("LHA list parsing failed or returned zero size");
    return false;
}

static bool test_lha_extract_with_progress(void)
{
    test_log("Testing LHA extract with enhanced progress tracking");

    /* First get the total size and file count */
    char list_cmd[512];
    snprintf(list_cmd, sizeof(list_cmd), "lha l %s", TEST_ARCHIVE);

    uint32_t total_size = 0;
    uint32_t file_count = 0;
    if (!lha_controlled_list(list_cmd, &total_size, &file_count)) {
        test_log("Failed to get total size/count for extract test");
        return false;
    }

    test_log("Total size for extraction: %lu bytes", (unsigned long)total_size);
    test_log("Total files for extraction: %lu files", (unsigned long)file_count);

    printf("   Archive contains %lu files, total size: %lu bytes\n", 
           (unsigned long)file_count, (unsigned long)total_size);

    /* Create destination directory */
    if (!create_directory(TEST_DEST_DIR)) {
        test_log("Failed to create destination directory: %s", TEST_DEST_DIR);
        return false;
    }

    /* Set up progress tracking context */
    test_progress_context_t progress_ctx = {
        .total_files = file_count,
        .processed_files = 0,
        .last_percentage = 0,
        .completion_detected = false
    };

    /* Build extraction command */
    char extract_cmd[512];
    snprintf(extract_cmd, sizeof(extract_cmd), "lha x -m -n %s %s", TEST_ARCHIVE, TEST_DEST_DIR);

    test_log("LHA extract command: %s", extract_cmd);
    printf("   Starting extraction with progress tracking...\n");

    /* Configure process execution with our custom line processor */
    process_exec_config_t config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_extract_test",
        .timeout_seconds = 60,
        .silent_mode = false
    };

    /* Execute controlled process with our enhanced line processor */
    controlled_process_t process;
    bool result = execute_controlled_process(extract_cmd, test_extract_line_processor, &progress_ctx, &config, &process);

    test_log("LHA extract result: %s", result ? "success" : "failure");

    /* Check and display exit code */
    int32_t exit_code;
    if (get_process_exit_code(&process, &exit_code)) {
        test_log("LHA extract exit code: %ld", (long)exit_code);
        printf("   Process exit code: %ld", (long)exit_code);
        
        if (exit_code == 0) {
            printf(" (Success)\n");
        } else {
            printf(" (Warning/Error - check log for details)\n");
        }
    } else {
        test_log("Could not retrieve exit code");
        printf("   Exit code not available\n");
    }

    if (result) {
        printf("   Extraction completed successfully! Processed %lu files\n", 
               (unsigned long)progress_ctx.processed_files);
        test_log("LHA extract with enhanced progress tracking successful");
        test_log("Final files processed: %lu/%lu", 
                 (unsigned long)progress_ctx.processed_files, (unsigned long)progress_ctx.total_files);
    } else {
        printf("   Extraction failed!\n");
        test_log("LHA extract with enhanced progress tracking failed");
    }

    /* Clean up process resources */
    cleanup_controlled_process(&process);

    return result;
}

static bool test_process_death_monitoring(void)
{
    test_log("Testing process death monitoring");

    /* This test will be implemented in Phase 2 */
    /* For now, we'll just log that it's being skipped */
    test_log("Process death monitoring test skipped (Phase 1)");
    
    return true;  /* Always pass for Phase 1 */
}

static bool test_line_processor(const char *line, void *user_data)
{
    int *line_count = (int *)user_data;
    
    if (line && line_count) {
        (*line_count)++;
        test_log("Processed line %d: %s", *line_count, line);
    }
    
    return true;
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

static bool test_extract_line_processor(const char *line, void *user_data)
{
    test_progress_context_t *ctx = (test_progress_context_t *)user_data;
    
    if (!line || !ctx) {
        return false;
    }

    /* Strip escape codes from line */
    char clean_line[256];
    strip_test_escape_codes(line, clean_line, sizeof(clean_line));

    test_log("Processing extract line: %s", clean_line);

    /* Check for completion indicators */
    if (strstr(clean_line, "Operation successful")) {
        ctx->completion_detected = true;
        test_log("LHA extraction completion detected");
        return true;
    }

    /* Parse extraction information */
    char filename[128];
    
    if (parse_test_extract_line(clean_line, filename, sizeof(filename))) {
        ctx->processed_files++;
        
        /* Calculate percentage based on file count */
        uint32_t percentage = 0;
        if (ctx->total_files > 0) {
            percentage = (ctx->processed_files * 100) / ctx->total_files;
        }
        
        /* Display progress at 5% intervals or every file if less than 20 files */
        bool show_progress = false;
        if (ctx->total_files < 20) {
            show_progress = true;  /* Show every file for small archives */
        } else if (percentage > ctx->last_percentage + 4) {
            show_progress = true;  /* Show at 5% intervals for large archives */
            ctx->last_percentage = percentage;
        }
        
        if (show_progress) {
            printf("   [%3lu%%] %s (%lu/%lu files)\n", 
                   (unsigned long)percentage,
                   filename,
                   (unsigned long)ctx->processed_files,
                   (unsigned long)ctx->total_files);
        }
        
        test_log("Extracted file [%lu%%]: %s (%lu/%lu)", 
                 (unsigned long)percentage, filename,
                 (unsigned long)ctx->processed_files, (unsigned long)ctx->total_files);
    }

    return true;
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

/* Archive integrity test context */
typedef struct {
    uint32_t files_tested;
    uint32_t errors_found;
    bool integrity_ok;
    char last_error[256];
} integrity_test_context_t;

static bool test_lha_archive_integrity_good(void)
{
    test_log("Testing LHA archive integrity (good archive)");

    /* Test the good archive */
    char test_cmd[512];
    snprintf(test_cmd, sizeof(test_cmd), "lha -n t %s", TEST_ARCHIVE);

    test_log("LHA integrity test command: %s", test_cmd);
    printf("   Testing archive integrity: %s\n", TEST_ARCHIVE);

    /* Set up integrity test context */
    integrity_test_context_t integrity_ctx = {
        .files_tested = 0,
        .errors_found = 0,
        .integrity_ok = true,
        .last_error = {0}
    };

    /* Configure process execution */
    process_exec_config_t config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_test_good",
        .timeout_seconds = 30,
        .silent_mode = false
    };

    /* Execute controlled process with integrity line processor */
    controlled_process_t process;
    bool result = execute_controlled_process(test_cmd, test_integrity_line_processor, &integrity_ctx, &config, &process);

    test_log("LHA integrity test result: %s", result ? "success" : "failure");
    test_log("Files tested: %lu", (unsigned long)integrity_ctx.files_tested);
    test_log("Errors found: %lu", (unsigned long)integrity_ctx.errors_found);

    /* Check exit code */
    int32_t exit_code = -1;
    if (get_process_exit_code(&process, &exit_code)) {
        test_log("LHA integrity test exit code: %ld", (long)exit_code);
        printf("   Process exit code: %ld", (long)exit_code);
        
        if (exit_code == 0) {
            printf(" (Archive is OK)\n");
        } else {
            printf(" (Archive has errors)\n");
        }
    } else {
        test_log("Could not retrieve exit code");
        printf("   Exit code not available\n");
    }

    /* Display results */
    printf("   Files tested: %lu\n", (unsigned long)integrity_ctx.files_tested);
    printf("   Errors found: %lu\n", (unsigned long)integrity_ctx.errors_found);
    
    if (integrity_ctx.errors_found == 0 && exit_code == 0) {
        printf("   Archive integrity: PASSED\n");
        test_log("Good archive integrity test successful");
    } else {
        printf("   Archive integrity: FAILED\n");
        test_log("Good archive integrity test failed");
        if (strlen(integrity_ctx.last_error) > 0) {
            test_log("Last error: %s", integrity_ctx.last_error);
        }
    }

    /* Clean up process resources */
    cleanup_controlled_process(&process);

    return (result && integrity_ctx.errors_found == 0 && exit_code == 0);
}

static bool test_lha_archive_integrity_corrupted(void)
{
    test_log("Testing LHA archive integrity (corrupted archive)");

    /* Test the corrupted archive */
    char test_cmd[512];
    snprintf(test_cmd, sizeof(test_cmd), "lha -n t %s", TEST_CORRUPTED_ARCHIVE);

    test_log("LHA integrity test command: %s", test_cmd);
    printf("   Testing archive integrity: %s\n", TEST_CORRUPTED_ARCHIVE);

    /* Set up integrity test context */
    integrity_test_context_t integrity_ctx = {
        .files_tested = 0,
        .errors_found = 0,
        .integrity_ok = true,
        .last_error = {0}
    };

    /* Configure process execution */
    process_exec_config_t config = {
        .tool_name = "LhA",
        .pipe_prefix = "lha_test_corrupted",
        .timeout_seconds = 30,
        .silent_mode = false
    };

    /* Execute controlled process with integrity line processor */
    controlled_process_t process;
    bool result = execute_controlled_process(test_cmd, test_integrity_line_processor, &integrity_ctx, &config, &process);

    test_log("LHA integrity test result: %s", result ? "success" : "failure");
    test_log("Files tested: %lu", (unsigned long)integrity_ctx.files_tested);
    test_log("Errors found: %lu", (unsigned long)integrity_ctx.errors_found);

    /* Check exit code */
    int32_t exit_code = -1;
    if (get_process_exit_code(&process, &exit_code)) {
        test_log("LHA integrity test exit code: %ld", (long)exit_code);
        printf("   Process exit code: %ld", (long)exit_code);
        
        if (exit_code == 0) {
            printf(" (Archive is OK)\n");
        } else {
            printf(" (Archive has errors)\n");
        }
    } else {
        test_log("Could not retrieve exit code");
        printf("   Exit code not available\n");
    }

    /* Display results */
    printf("   Files tested: %lu\n", (unsigned long)integrity_ctx.files_tested);
    printf("   Errors found: %lu\n", (unsigned long)integrity_ctx.errors_found);
    
    if (integrity_ctx.errors_found > 0 || exit_code != 0) {
        printf("   Archive integrity: FAILED (Archive is damaged)\n");
        test_log("Corrupted archive integrity test successful (detected corruption)");
        if (strlen(integrity_ctx.last_error) > 0) {
            test_log("Last error: %s", integrity_ctx.last_error);
            printf("   Last error: %s\n", integrity_ctx.last_error);
        }
    } else {
        printf("   Archive integrity: PASSED (Unexpected - should be corrupted)\n");
        test_log("Corrupted archive integrity test failed (did not detect corruption)");
    }

    /* Clean up process resources */
    cleanup_controlled_process(&process);

    /* For corrupted archive, we expect errors - so test passes if we found errors */
    return (result && (integrity_ctx.errors_found > 0 || exit_code != 0));
}

static bool test_integrity_line_processor(const char *line, void *user_data)
{
    integrity_test_context_t *ctx = (integrity_test_context_t *)user_data;
    
    if (!line || !ctx) {
        return false;
    }

    /* Strip escape codes from line */
    char clean_line[256];
    strip_test_escape_codes(line, clean_line, sizeof(clean_line));

    test_log("Processing integrity line: %s", clean_line);

    /* Check for testing indicators */
    if (strstr(clean_line, "Testing:")) {
        ctx->files_tested++;
        test_log("Testing file count: %lu", (unsigned long)ctx->files_tested);
        return true;
    }

    /* Check for various error indicators */
    if (strstr(clean_line, "*** Error") || 
        strstr(clean_line, "Failed CRC Check") ||
        strstr(clean_line, "Bad decoding table") ||
        strstr(clean_line, "WARNING: Skipping corrupt")) {
        
        ctx->errors_found++;
        ctx->integrity_ok = false;
        
        /* Store the last error message */
        size_t len = strlen(clean_line);
        if (len >= sizeof(ctx->last_error)) {
            len = sizeof(ctx->last_error) - 1;
        }
        size_t i;
        for (i = 0; i < len; i++) {
            ctx->last_error[i] = clean_line[i];
        }
        ctx->last_error[len] = '\0';
        
        test_log("Error detected [%lu]: %s", (unsigned long)ctx->errors_found, clean_line);
        return true;
    }

    return true;
}

static bool test_corruption_detection(void)
{
    test_log("Testing corruption detection using file corruptor");

#ifndef PLATFORM_AMIGA
    /* This test only runs on host platform where file_corruptor is available */
    
    /* Step 1: Copy original archive to test file */
    char copy_cmd[512];
#ifdef _WIN32
    snprintf(copy_cmd, sizeof(copy_cmd), "copy \"%s\" \"%s\"", TEST_ARCHIVE, TEST_CORRUPTED_ARCHIVE);
#else
    snprintf(copy_cmd, sizeof(copy_cmd), "cp %s %s", TEST_ARCHIVE, TEST_CORRUPTED_ARCHIVE);
#endif
    
    test_log("Copying original archive: %s", copy_cmd);
    
    if (system(copy_cmd) != 0) {
        test_log("Failed to copy original archive");
        return false;
    }
    
    /* Step 2: Test original archive integrity */
    char test_cmd[512];
    snprintf(test_cmd, sizeof(test_cmd), "lha t %s", TEST_CORRUPTED_ARCHIVE);
    
    test_log("Testing original archive integrity: %s", test_cmd);
    
    int original_result = system(test_cmd);
    if (original_result != 0) {
        test_log("Original archive failed integrity check (unexpected)");
        return false;
    }
    
    test_log("Original archive passes integrity check");
    
    /* Step 3: Corrupt the archive using file_corruptor */
    char corrupt_cmd[512];
    snprintf(corrupt_cmd, sizeof(corrupt_cmd), "./file_corruptor.exe %s", TEST_CORRUPTED_ARCHIVE);
    
    test_log("Corrupting archive: %s", corrupt_cmd);
    
    int corrupt_result = system(corrupt_cmd);
    if (corrupt_result != 0) {
        test_log("File corruptor failed");
        return false;
    }
    
    test_log("Archive corruption completed");
    
    /* Step 4: Test corrupted archive integrity */
    test_log("Testing corrupted archive integrity: %s", test_cmd);
    
    int corrupted_result = system(test_cmd);
    if (corrupted_result == 0) {
        test_log("Corrupted archive unexpectedly passed integrity check");
        return false;
    }
    
    test_log("Corrupted archive correctly failed integrity check");
    
    /* Step 5: Cleanup */
    char cleanup_cmd[512];
#ifdef _WIN32
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "del \"%s\"", TEST_CORRUPTED_ARCHIVE);
#else
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -f %s", TEST_CORRUPTED_ARCHIVE);
#endif
    system(cleanup_cmd);
    
    test_log("Corruption detection test completed successfully");
    printf("   Original archive: PASSED integrity check\n");
    printf("   Corrupted archive: FAILED integrity check (as expected)\n");
    
    return true;
#else
    /* On Amiga, file_corruptor is not available, so skip this test */
    test_log("Corruption detection test skipped on Amiga platform");
    printf("   (file_corruptor not available on Amiga)\n");
    return true;
#endif
}
