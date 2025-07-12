/*------------------------------------------------------------------------*/
/*                                                                        *
 * main.c — Entry point for the Amiga CLI wrapper                        *
 * Amiga CLI Wrapper - Command-line utility framework                    *
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
 * @brief Print version information
 */
/*------------------------------------------------------------------------*/
static void print_version(void)
{
    printf("Amiga CLI Wrapper v1.0.0\n");
    printf("Dual-target command-line utilities for Amiga development\n");
    printf("Copyright (c) 2025 Kerry Thompson\n");
} /* print_version */

/*------------------------------------------------------------------------*/
/**
 * @brief Print usage information
 *
 * @param prog_name Program name from argv[0]
 */
/*------------------------------------------------------------------------*/
static void print_usage(const char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n");
    printf("\n");
    printf("Platform: ");
#ifdef PLATFORM_AMIGA
    printf("Amiga (vbcc +aos68k)\n");
#else
    printf("Host (development)\n");
#endif
    printf("Build: " __DATE__ " " __TIME__ "\n");
} /* print_usage */

/*------------------------------------------------------------------------*/
/**
 * @brief Main entry point
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return int Exit code (0 for success, non-zero for error)
 */
/*------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    int i;              /* Loop counter */
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        } /* if */
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            print_version();
            return 0;
        } /* else if */
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } /* else */
    } /* for */

    /* No arguments provided - show basic info */
    printf("Amiga CLI Wrapper - Ready\n");
    printf("Platform: ");
#ifdef PLATFORM_AMIGA
    printf("Amiga\n");
#else
    printf("Host\n");
#endif
    printf("Use --help for more information\n");

    return 0;
} /* main */

/* End of Text */