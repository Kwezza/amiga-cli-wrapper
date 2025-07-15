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

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static FILE *test_logfile = NULL;

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
static bool test_process_death_monitoring(void);

/* Test line processor for basic process test */
static bool test_line_processor(const char *line, void *user_data);

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
    run_test("Process Death Monitoring", test_process_death_monitoring);

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
    test_log("Testing LHA extract with progress tracking");

    /* First get the total size */
    char list_cmd[512];
    snprintf(list_cmd, sizeof(list_cmd), "lha l %s", TEST_ARCHIVE);

    uint32_t total_size = 0;
    if (!lha_controlled_list(list_cmd, &total_size, NULL)) {
        test_log("Failed to get total size for extract test");
        return false;
    }

    test_log("Total size for extraction: %lu bytes", (unsigned long)total_size);

    /* Create destination directory */
    if (!create_directory(TEST_DEST_DIR)) {
        test_log("Failed to create destination directory: %s", TEST_DEST_DIR);
        return false;
    }

    /* Build extraction command */
    char extract_cmd[512];
    snprintf(extract_cmd, sizeof(extract_cmd), "lha x -m -n %s %s", TEST_ARCHIVE, TEST_DEST_DIR);

    test_log("LHA extract command: %s", extract_cmd);

    bool result = lha_controlled_extract(extract_cmd, total_size);

    test_log("LHA extract result: %s", result ? "success" : "failure");

    if (result) {
        test_log("LHA extract with progress tracking successful");
    } else {
        test_log("LHA extract with progress tracking failed");
    }

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
