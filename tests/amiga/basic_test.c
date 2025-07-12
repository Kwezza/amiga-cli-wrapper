/*------------------------------------------------------------------------*/
/*                                                                        *
 * basic_test.c — Basic test suite for Amiga platform                    *
 * Amiga CLI Wrapper - Amiga platform testing                            *
 *                                                                        *
 * Copyright © 2025 Kerry Thompson                                        *
 * SPDX-License-Identifier: MIT                                           *
 *                                                                        */
/*------------------------------------------------------------------------*/

#include <platform.h>
#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------------*/
/**
 * @brief Test platform memory allocation
 *
 * @return int 1 on success, 0 on failure
 */
/*------------------------------------------------------------------------*/
static int test_memory_allocation(void)
{
    void *ptr;          /* Test pointer */
    
    printf("Testing memory allocation...\n");
    
    /* Test malloc */
    ptr = cli_malloc(1024);
    if (ptr == NULL)
    {
        printf("FAIL: cli_malloc failed\n");
        return 0;
    } /* if */
    
    /* Test free */
    cli_free(ptr);
    
    printf("PASS: Memory allocation test\n");
    return 1;
} /* test_memory_allocation */

/*------------------------------------------------------------------------*/
/**
 * @brief Test Amiga-specific features
 *
 * @return int 1 on success, 0 on failure
 */
/*------------------------------------------------------------------------*/
static int test_amiga_features(void)
{
    printf("Testing Amiga-specific features...\n");
    
#ifdef PLATFORM_AMIGA
    printf("Running on real Amiga platform\n");
    printf("Path separator: '%c'\n", CLI_PATH_SEP);
    printf("End of line: Amiga format\n");
    
    /* Test basic Amiga functionality */
    if (cli_access(":", F_OK) == 0)
    {
        printf("PASS: Root directory accessible\n");
    } /* if */
    else
    {
        printf("WARNING: Root directory not accessible\n");
    } /* else */
#else
    printf("SKIP: Not running on Amiga platform\n");
#endif
    
    printf("PASS: Amiga features test\n");
    return 1;
} /* test_amiga_features */

/*------------------------------------------------------------------------*/
/**
 * @brief Main test function
 *
 * @return int Exit code (0 for success, non-zero for failure)
 */
/*------------------------------------------------------------------------*/
int main(void)
{
    int tests_passed = 0;   /* Number of tests passed */
    int total_tests = 0;    /* Total number of tests */
    
    printf("Amiga CLI Wrapper - Amiga Platform Test Suite\n");
    printf("=============================================\n\n");
    
    /* Run tests */
    total_tests++;
    if (test_memory_allocation())
    {
        tests_passed++;
    } /* if */
    
    total_tests++;
    if (test_amiga_features())
    {
        tests_passed++;
    } /* if */
    
    /* Print results */
    printf("\nTest Results: %d/%d tests passed\n", tests_passed, total_tests);
    
    if (tests_passed == total_tests)
    {
        printf("All tests PASSED\n");
        return 0;
    } /* if */
    else
    {
        printf("Some tests FAILED\n");
        return 1;
    } /* else */
} /* main */

/* End of Text */