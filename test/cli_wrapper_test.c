#include "cli_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wait_for_run_complete(void);
static bool review_logfile(void);

int main(int argc, char *argv[])
{
    (void)argc; /* Unused parameter */
    (void)argv; /* Unused parameter */

    printf("CLI Wrapper Test Program\n");
    printf("========================\n\n");

    printf("This program tests the CLI wrapper functionality in two phases:\n");
    printf("1. Host testing (stubbed functionality for verification)\n");
    printf("2. Emulator testing (full Amiga functionality)\n\n");

    printf("The Amiga executable has been built at: build/amiga/cli_wrapper_test\n");
    printf("Copy this to your Amiga emulator and run it there.\n\n");

    /* Initialize the CLI wrapper */
    if (!cli_wrapper_init()) {
        printf("ERROR: Failed to initialize CLI wrapper\n");
        printf("This could be due to:\n");
        printf("1. No write permission in current directory\n");
        printf("2. Insufficient memory\n");
        printf("3. File system issues\n");
        return 1;
    }

    printf("CLI wrapper initialized successfully!\n");
    printf("Step 1: Listing archive contents...\n");
    printf("Looking for: assets/A10TankKiller_v2.0_3Disk.lha\n");

    uint32_t total_size = 0;
    bool list_result = cli_list("lha l assets/A10TankKiller_v2.0_3Disk.lha", &total_size);

    printf("List operation result: %s\n", list_result ? "SUCCESS" : "FAILED");
    if (list_result) {
        printf("Total uncompressed size: %lu bytes\n", (unsigned long)total_size);
    }

    printf("\nStep 2: Waiting for emulator run...\n");
    wait_for_run_complete();

    printf("\nStep 3: Extracting archive...\n");

    bool extract_result = cli_extract("lha x -m -n assets/A10TankKiller_v2.0_3Disk.lha test/", total_size);

    printf("Extract operation result: %s\n", extract_result ? "SUCCESS" : "FAILED");

    printf("\nStep 4: Reviewing log file...\n");

    bool review_result = review_logfile();

    printf("\nFinal Results:\n");
    printf("=============\n");
    printf("List operation:    %s\n", list_result ? "PASS" : "FAIL");
    printf("Extract operation: %s\n", extract_result ? "PASS" : "FAIL");
    printf("Log file review:   %s\n", review_result ? "PASS" : "FAIL");
    printf("Overall status:    %s\n",
           (list_result && extract_result && review_result) ? "SUCCESS" : "FAILED");

    /* Cleanup */
    cli_wrapper_cleanup();

    return (list_result && extract_result && review_result) ? 0 : 1;
}

static void wait_for_run_complete(void)
{
    char input[256];

    printf("\n");
    printf("========================================\n");
    printf("EMULATOR RUN REQUIRED\n");
    printf("========================================\n");
    printf("Please run the generated executable on your Amiga emulator.\n");
    printf("After the program completes, enter 'RUN_COMPLETE' to continue: ");
    fflush(stdout);

    while (1) {
        if (fgets(input, sizeof(input), stdin)) {
            /* Remove trailing newline */
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') {
                input[len-1] = '\0';
            }

            if (strcmp(input, "RUN_COMPLETE") == 0) {
                printf("Continuing with log file analysis...\n");
                break;
            } else {
                printf("Please enter exactly 'RUN_COMPLETE' to continue: ");
                fflush(stdout);
            }
        }
    }
}

static bool review_logfile(void)
{
    FILE *logfile = fopen("logfile.txt", "r");
    if (!logfile) {
        printf("ERROR: Could not open logfile.txt for review\n");
        return false;
    }

    printf("Analyzing logfile.txt...\n");

    char line[512];
    int total_lines = 0;
    int list_parsed_count = 0;
    int extract_parsed_count = 0;
    bool found_100_percent = false;
    bool found_child_exit = false;
    bool found_timing_info = false;
    bool found_session_start = false;
    bool found_session_end = false;

    while (fgets(line, sizeof(line), logfile)) {
        total_lines++;

        /* Check for key indicators */
        if (strstr(line, "=== CLI Wrapper Session Started ===")) {
            found_session_start = true;
        } else if (strstr(line, "=== CLI Wrapper Session Ended ===")) {
            found_session_end = true;
        } else if (strstr(line, "LIST_PARSED:")) {
            list_parsed_count++;
        } else if (strstr(line, "EXTRACT_PARSED:")) {
            extract_parsed_count++;
        } else if (strstr(line, "100.0%") || strstr(line, "percentage=100.0")) {
            found_100_percent = true;
        } else if (strstr(line, "SystemTagList returned:") || strstr(line, "EXECUTE_")) {
            found_child_exit = true;
        } else if (strstr(line, "TIMING:")) {
            found_timing_info = true;
        }
    }

    fclose(logfile);

    /* Print analysis results */
    printf("\nLog File Analysis Results:\n");
    printf("--------------------------\n");
    printf("Total log lines:           %d\n", total_lines);
    printf("Session markers:           %s\n",
           (found_session_start && found_session_end) ? "FOUND" : "MISSING");
    printf("List parsed entries:       %d\n", list_parsed_count);
    printf("Extract parsed entries:    %d\n", extract_parsed_count);
    printf("100%% completion marker:    %s\n", found_100_percent ? "FOUND" : "MISSING");
    printf("Command execution logged:  %s\n", found_child_exit ? "FOUND" : "MISSING");
    printf("Timing information:        %s\n", found_timing_info ? "FOUND" : "MISSING");

    /* Determine overall success */
    bool analysis_success = (total_lines >= 10 &&
                            found_session_start &&
                            list_parsed_count > 0 &&
                            extract_parsed_count > 0 &&
                            found_child_exit);

    printf("\nAnalysis Summary:\n");
    if (analysis_success) {
        printf("* Log file contains expected debug information\n");
        if (found_100_percent) {
            printf("* Extraction reached 100%% completion\n");
        } else {
            printf("! Extraction may not have reached 100%% (check manually)\n");
        }
        if (found_timing_info) {
            printf("* Performance timing information recorded\n");
        }
    } else {
        printf("X Log file missing critical debug information\n");

        if (list_parsed_count == 0) {
            printf("  - No list operation parsing detected\n");
        }
        if (extract_parsed_count == 0) {
            printf("  - No extract operation parsing detected\n");
        }
        if (!found_child_exit) {
            printf("  - No command execution logging detected\n");
        }
        if (total_lines < 10) {
            printf("  - Log file appears too short (%d lines)\n", total_lines);
        }
    }

    return analysis_success;
}
