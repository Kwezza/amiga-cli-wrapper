/* CLI Wrapper Test - Final Working Version */
#include "cli_wrapper.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_AMIGA
#include <proto/dos.h>
#include <dos/dos.h>
#endif

static void wait_for_input(void) {
    printf("\n");
    printf("===========================================\n");
    printf("PROGRAM COMPLETED SUCCESSFULLY\n");
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
    printf("CLI WRAPPER TEST - FINAL VERSION\n");
    printf("=========================================\n");
    printf("Testing LHA archive operations on Amiga\n");
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
    
    list_ok = cli_list("lha l assets/A10TankKiller_v2.0_3Disk.lha", &total_size);
    
    if (list_ok) {
        printf("SUCCESS: Archive listing completed\n");
        printf("- Files detected and processed\n");
        printf("- Total uncompressed size: %lu bytes\n", (unsigned long)total_size);
        printf("- Size in KB: %lu KB\n", (unsigned long)(total_size / 1024));
        if (total_size > 1048576) {
            printf("- Size in MB: %lu MB\n", (unsigned long)(total_size / (1024 * 1024)));
        }
    } else {
        printf("FAILED: Archive listing failed\n");
        printf("- Check if archive file exists\n");
        printf("- Check if LHA command is available\n");
    }
    printf("\n");
    fflush(stdout);
    
    /* Step 3: Extract archive */
    if (list_ok && total_size > 0) {
        printf("Step 3: Extracting archive...\n");
        printf("Target directory: test/\n");
        printf("Command: lha x -m -n\n");
        printf("Processing (this may take a moment)...\n");
        fflush(stdout);
        
        extract_ok = cli_extract("lha x -m -n assets/A10TankKiller_v2.0_3Disk.lha test/", total_size);
        
        if (extract_ok) {
            printf("SUCCESS: Archive extraction completed\n");
            printf("- All files extracted to test/ directory\n");
            printf("- Check the test/ folder for extracted files\n");
        } else {
            printf("FAILED: Archive extraction failed\n");
            printf("- Check available disk space\n");
            printf("- Check write permissions\n");
        }
    } else {
        extract_ok = false;
        printf("Step 3: SKIPPED (list operation failed)\n");
    }
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
    printf("Archive Listing:    %s\n", list_ok ? "PASS" : "FAIL");
    printf("Archive Extraction: %s\n", extract_ok ? "PASS" : "FAIL");
    
    if (list_ok && extract_ok) {
        printf("\nOVERALL RESULT: SUCCESS!\n");
        printf("\nThe CLI wrapper is working correctly!\n");
        printf("- LHA archive listing works\n");
        printf("- LHA archive extraction works\n");
        printf("- All operations completed successfully\n");
        printf("\nYou can now use the CLI wrapper functions:\n");
        printf("- cli_list() for listing archive contents\n");
        printf("- cli_extract() for extracting archives\n");
    } else {
        printf("\nOVERALL RESULT: PARTIAL SUCCESS\n");
        if (list_ok && !extract_ok) {
            printf("- Listing works, but extraction failed\n");
            printf("- Check disk space and permissions\n");
        } else {
            printf("- Archive listing failed\n");
            printf("- Check archive file and LHA availability\n");
        }
    }
    
    printf("\nCheck logfile.txt for detailed operation logs.\n");
    
    wait_for_input();
    return (list_ok && extract_ok) ? 0 : 1;
}
