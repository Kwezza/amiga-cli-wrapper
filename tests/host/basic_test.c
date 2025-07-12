/*------------------------------------------------------------------------*/
/*                                                                        *
 * basic_test.c — Basic test suite for host platform                     *
 * Amiga CLI Wrapper - Host platform testing                             *
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
 * @brief Test platform constants
 *
 * @return int 1 on success, 0 on failure
 */
/*------------------------------------------------------------------------*/
static int test_platform_constants(void)
{
    printf("Testing platform constants...\n");
    
    printf("CLI_PATH_SEP: '%c'\n", CLI_PATH_SEP);
    printf("CLI_EOL: \"%s\"\n", CLI_EOL);
    
    /* Basic sanity checks */
    if (CLI_PATH_SEP != '/' && CLI_PATH_SEP != '\\')
    {
        printf("FAIL: Invalid path separator\n");
        return 0;
    } /* if */
    
    if (strcmp(CLI_EOL, "\n") != 0 && strcmp(CLI_EOL, "\r\n") != 0)
    {
        printf("FAIL: Invalid end-of-line sequence\n");
        return 0;
    } /* if */
    
    printf("PASS: Platform constants test\n");
    return 1;
} /* test_platform_constants */

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
    
    printf("Amiga CLI Wrapper - Host Platform Test Suite\n");
    printf("============================================\n\n");
    
    /* Run tests */
    total_tests++;
    if (test_memory_allocation())
    {
        tests_passed++;
    } /* if */
    
    total_tests++;
    if (test_platform_constants())
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