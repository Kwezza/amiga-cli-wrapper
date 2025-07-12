/*------------------------------------------------------------------------*/
/*                                                                        *
 * platform_mem.h — Memory allocation abstraction                        *
 * Amiga CLI Wrapper - Cross-platform memory management                  *
 *                                                                        *
 * Copyright © 2025 Kerry Thompson                                        *
 * SPDX-License-Identifier: MIT                                           *
 *                                                                        */
/*------------------------------------------------------------------------*/

#ifndef PLATFORM_MEM_H
#define PLATFORM_MEM_H

#ifdef PLATFORM_AMIGA
    /* Amiga target - use standard malloc/free (vbcc provides these) */
    #include <stdlib.h>
    #define cli_malloc(sz)  malloc(sz)
    #define cli_free(p)     free(p)
    #define cli_realloc(p, sz) realloc(p, sz)
    #define cli_calloc(n, sz) calloc(n, sz)
#else
    /* Host target - use standard C library */
    #include <stdlib.h>
    #define cli_malloc(sz)  malloc(sz)
    #define cli_free(p)     free(p)
    #define cli_realloc(p, sz) realloc(p, sz)
    #define cli_calloc(n, sz) calloc(n, sz)
#endif

#endif /* PLATFORM_MEM_H */

/* End of Text */