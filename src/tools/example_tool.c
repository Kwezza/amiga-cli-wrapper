/*------------------------------------------------------------------------*/
/*                                                                        *
 * example_tool.c — Example CLI tool implementation                      *
 * Amiga CLI Wrapper - Sample command-line utility                       *
 *                                                                        *
 * Copyright © 2025 Kerry Thompson                                        *
 * SPDX-License-Identifier: MIT                                           *
 *                                                                        */
/*------------------------------------------------------------------------*/

#include <platform.h>
#include <stdio.h>

/*------------------------------------------------------------------------*/
/**
 * @brief Example tool function
 *
 * This is a placeholder for an actual CLI tool implementation.
 * Real tools would provide specific functionality.
 */
/*------------------------------------------------------------------------*/
void example_tool_function(void)
{
    printf("Example tool function called\n");
    printf("Platform: ");
#ifdef PLATFORM_AMIGA
    printf("Amiga\n");
#else
    printf("Host\n");
#endif
} /* example_tool_function */

/* End of Text */