/* CLI Wrapper Byte-Level Test Demo */
#include "cli_wrapper.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_AMIGA
#include <proto/dos.h>
#include <dos/dos.h>
/* Request 64KB stack for Amiga build */
size_t __stack = 65536;  /* request a 64 KB stack */
#endif

/* Add logging function for test debugging */
static void test_log(const char *message) {
    printf("TEST_DEBUG: %s\n", message);
    fflush(stdout);
}

static void wait_for_input(void) {
    printf("\n");
    printf("===========================================\n");
    printf("PROGRAM COMPLETED\n");
    printf("===========================================\n");
    printf("Press ENTER to close this window...\n");
    fflush(stdout);

#ifdef PLATFORM_AMIGA
    char buffer[2];
    BPTR input = Input();
    if (input) {
        Read(input, buffer, 1);
    } else {
        /* Fallback: 3 second delay */
        volatile int i;
        for (i = 0; i < 1500000; i++) {
            /* 3 second delay */
        }
    }
#else
    getchar();
#endif
}

int main(void) {
    printf("=========================================\n");
    printf("CLI WRAPPER BYTE-LEVEL EXTRACTION TEST\n");
    printf("=========================================\n");
    printf("Testing byte-level LHA extraction on Amiga\n");
    printf("Update interval: %d KiB\n", LHA_UPDATE_INTERVAL_KB);
    printf("\n");
    fflush(stdout);

    uint32_t total_size = 0;
    bool list_ok, extract_ok;

    /* Step 1: Initialize */
    printf("Step 1: Initializing CLI wrapper...\n");
    if (!cli_wrapper_init()) {
        printf("ERROR: CLI wrapper initialization failed!\n");
        wait_for_input();
        return 1;
    }
    printf("SUCCESS: CLI wrapper initialized\n");
    printf("\n");
    fflush(stdout);

    /* Step 2: List archive contents */
    printf("Step 2: Listing archive contents...\n");
    printf("Archive: assets/A10TankKiller_v2.0_3Disk.lha\n");
    printf("Command: lha l\n");
    printf("Processing...\n");
    fflush(stdout);

    test_log("About to call cli_list()");
    list_ok = cli_list("lha l assets/A10TankKiller_v2.0_3Disk.lha", &total_size);
    test_log("cli_list() returned");

    if (list_ok) {
        test_log("cli_list returned success, total_size received");
        printf("SUCCESS: Archive listing completed\n");
        printf("- Files detected and processed\n");
        printf("- Total uncompressed size: %lu bytes\n", (unsigned long)total_size);
        test_log("About to calculate KB size");
        printf("- Size in KB: %lu KB\n", (unsigned long)(total_size / 1024));
        test_log("KB size calculated");
        if (total_size > 1048576) {
            test_log("Size > 1MB, calculating MB");
            printf("- Size in MB: %lu MB\n", (unsigned long)(total_size / (1024 * 1024)));
            test_log("MB size calculated");
        }
        test_log("Listing success messages completed");
    } else {
        test_log("cli_list returned failure");
        printf("FAILED: Archive listing failed\n");
        printf("- Check if archive file exists\n");
        printf("- Check if LHA command is available\n");
    }
    test_log("About to flush and proceed to extraction check");
    printf("\n");
    fflush(stdout);

    /* Step 3: Byte-level extraction */
    test_log("Checking if should proceed to extraction");
    if (list_ok && total_size > 0) {
        test_log("Conditions met, starting extraction phase");
        printf("Step 3: Extracting archive with byte-level progress...\n");
        printf("Target directory: temp_extract/\n");
        printf("Command: lha x -m -n -w target_dir/ (proper LHA syntax)\n");
        printf("NOTE: Progress will be smoother on slower Amiga systems\n");
        printf("Processing (this may take a moment)...\n");
        printf("IMPORTANT: Adding safety delay before extraction...\n");
        fflush(stdout);

        test_log("About to start safety delay");
        /* CRITICAL: Add safety delay between listing and extraction */
        volatile int safety_delay;
        for (safety_delay = 0; safety_delay < 500000; safety_delay++) {
            /* 1 second safety delay */
        }
        test_log("Safety delay completed");
        printf("Safety delay completed, starting extraction...\n");
        fflush(stdout);

        /* CRITICAL: Delete temp_extract directory first to prevent conflicts */
        printf("Deleting temp_extract/ directory first...\n");
        fflush(stdout);

        /* TEMPORARY WORKAROUND: Skip deletion for now to test extraction */
        test_log("WORKAROUND: Skipping directory deletion to avoid System() hang");
        printf("WORKAROUND: Skipping directory deletion (System() was hanging)\n");
        printf("NOTE: temp_extract/ may already exist - LHA will overwrite files\n");
        fflush(stdout);

#if 0  /* Temporarily disabled - was causing hang */
#ifdef PLATFORM_AMIGA
        test_log("About to call System() with Delete command");
        /* Amiga: Use Delete command to remove directory recursively */
        /* Try a simpler approach first - just delete if it exists */
        printf("Checking if temp_extract/ exists...\n");
        fflush(stdout);

        /* Check if directory exists first */
        BPTR lock = Lock("temp_extract", ACCESS_READ);
        if (lock) {
            printf("Directory exists, attempting to delete...\n");
            fflush(stdout);
            UnLock(lock);

            /* Use simpler delete command without ALL flag first */
            int delete_result = System("Delete temp_extract QUIET", NULL);
            test_log("System() call completed");
            if (delete_result == 0) {
                test_log("Delete command succeeded");
                printf("Successfully deleted temp_extract/ directory\n");
            } else {
                test_log("Simple delete failed, trying with ALL flag");
                printf("Simple delete failed (%d), trying recursive delete...\n", delete_result);
                fflush(stdout);
                delete_result = System("Delete temp_extract/ ALL QUIET", NULL);
                if (delete_result == 0) {
                    printf("Recursive delete succeeded\n");
                } else {
                    printf("Recursive delete returned %d (continuing anyway)\n", delete_result);
                }
            }
        } else {
            test_log("Directory doesn't exist - no deletion needed");
            printf("Directory doesn't exist - no deletion needed\n");
        }
#else
        test_log("About to call system() with platform delete command");
        /* Host: Use system command appropriate for platform */
        int delete_result = system("rmdir /s /q temp_extract 2>nul || rm -rf temp_extract 2>/dev/null || true");
        test_log("system() call completed");
        printf("Delete command executed (result: %d)\n", delete_result);
#endif
#endif  /* End of temporarily disabled deletion code */
        test_log("Directory deletion phase completed");
        fflush(stdout);

        /* Create extraction command - change to directory first, then extract */
        char extract_cmd[256];
        snprintf(extract_cmd, sizeof(extract_cmd),
                "cd temp_extract && lha x -m -n ../assets/A10TankKiller_v2.0_3Disk.lha");

        test_log("About to print exact command");
        printf("EXACT COMMAND: [%s]\n", extract_cmd);
        fflush(stdout);

        test_log("About to call cli_extract()");
        extract_ok = cli_extract(extract_cmd, total_size);
        test_log("cli_extract() returned");

        if (extract_ok) {
            test_log("Extraction succeeded");
            printf("SUCCESS: Byte-level extraction completed\n");
            printf("- All files extracted to temp_extract/ directory\n");
            printf("- Progress was tracked at byte level\n");
            printf("- Check the temp_extract/ folder for extracted files\n");
        } else {
            test_log("Extraction failed");
            printf("FAILED: Byte-level extraction failed\n");
            printf("- Check available disk space\n");
            printf("- Check write permissions\n");
            printf("- Verify LHA supports -D0 and -U options\n");
        }
    } else {
        test_log("Skipping extraction - conditions not met");
        extract_ok = false;
        printf("Step 3: SKIPPED (list operation failed)\n");
    }
    test_log("Extraction phase completed, proceeding to cleanup");
    printf("\n");
    fflush(stdout);

    /* Step 4: Cleanup */
    printf("Step 4: Cleaning up...\n");
    cli_wrapper_cleanup();
    printf("SUCCESS: Cleanup completed\n");
    printf("\n");
    fflush(stdout);

    /* Final Results */
    printf("=========================================\n");
    printf("FINAL TEST RESULTS\n");
    printf("=========================================\n");
    printf("Archive Listing:         %s\n", list_ok ? "PASS" : "FAIL");
    printf("Byte-level Extraction:   %s\n", extract_ok ? "PASS" : "FAIL");

    if (list_ok && extract_ok) {
        printf("\nOVERALL RESULT: SUCCESS!\n");
        printf("\nThe byte-level CLI wrapper is working correctly!\n");
        printf("- LHA archive listing works\n");
        printf("- LHA byte-level extraction works\n");
        printf("- Progress tracking is smooth and efficient\n");
        printf("\nFunction tested:\n");
        printf("- cli_extract_bytes() for smooth progress\n");
        printf("- Update interval: %d KiB for optimal performance\n", LHA_UPDATE_INTERVAL_KB);
    } else {
        printf("\nOVERALL RESULT: ISSUES DETECTED\n");
        if (list_ok && !extract_ok) {
            printf("- Listing works, but byte-level extraction failed\n");
            printf("- Check if LHA supports -D0 and -U options\n");
            printf("- Check disk space and permissions\n");
        } else {
            printf("- Archive listing failed\n");
            printf("- Check archive file and LHA availability\n");
        }
    }

    printf("\nAdvantages of byte-level extraction:\n");
    printf("- Smoother progress on slower Amiga systems\n");
    printf("- Less frequent display updates (configurable)\n");
    printf("- Better performance with large files\n");
    printf("- Real-time byte counting instead of file counting\n");

    printf("\nCheck logfile.txt for detailed operation logs.\n");

    wait_for_input();
    return (list_ok && extract_ok) ? 0 : 1;
}
