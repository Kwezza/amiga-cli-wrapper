#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(void)
{
    FILE *logfile = fopen("logfile.txt", "r");
    if (!logfile) {
        printf("ERROR: Could not open logfile.txt for review\n");
        return 1;
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
            printf("DEBUG: Found session start at line %d\n", total_lines);
        } else if (strstr(line, "=== CLI Wrapper Session Ended ===")) {
            found_session_end = true;
            printf("DEBUG: Found session end at line %d\n", total_lines);
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
        printf("* CLI wrapper executed successfully on Amiga!\n");
        printf("* List operation: %d files processed\n", list_parsed_count);
        printf("* Extract operation: %d files extracted\n", extract_parsed_count);
        printf("* Real-time parsing and logging working correctly\n");
        if (found_timing_info) {
            printf("* Performance timing information recorded\n");
        }
    } else {
        printf("X Log file missing critical debug information\n");
    }

    return analysis_success ? 0 : 1;
}
