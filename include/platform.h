/*------------------------------------------------------------------------*/
/*                                                                        *
 * platform.h — Platform abstraction layer for dual-target builds       *
 * Amiga CLI Wrapper - Support both Amiga and host platforms             *
 *                                                                        *
 * Copyright © 2025 Kerry Thompson                                        *
 * SPDX-License-Identifier: MIT                                           *
 *                                                                        */
/*------------------------------------------------------------------------*/

#ifndef PLATFORM_H
#define PLATFORM_H

/* Include platform I/O wrappers */
#include "platform/platform_io.h"
/* Include memory allocation wrappers */
#include "platform/platform_mem.h"

/*------------------------------------------------------------------------*/
/* Platform-specific includes and definitions                            */
/*------------------------------------------------------------------------*/

#ifdef PLATFORM_AMIGA
    /* Real Amiga target - use AmigaOS APIs */
    #include <stdlib.h>
    #include <exec/types.h>
    #include <exec/memory.h>
    #include <utility/tagitem.h>
    #include <proto/exec.h>
    #include <stdint.h>

    /* vbcc with -c99 already provides stdint.h types, so don't redefine them */
    #if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
        /* Provide stdint.h compatible types for C89 builds */
        #ifndef uint8_t
            typedef unsigned char      uint8_t;
        #endif
        #ifndef uint16_t
            typedef unsigned short     uint16_t;
        #endif
        #ifndef uint32_t
            typedef unsigned long      uint32_t;
        #endif
        #ifndef int8_t
            typedef signed char        int8_t;
        #endif
        #ifndef int16_t
            typedef signed short       int16_t;
        #endif
        #ifndef int32_t
            typedef signed long        int32_t;
        #endif
    #endif

    /* Path and text formatting */
    #define CLI_PATH_SEP   '/'
    #define CLI_EOL        "\n"

#else /* PLATFORM_HOST */
    /* Host target - Linux/Windows/Raspberry Pi tools */
    #include <stdlib.h>
    #include <stdint.h>
    #include <stdbool.h>
    #include <string.h>
    #include <stdio.h>

    /* Host platform type definitions for compatibility */
    typedef unsigned char   UBYTE;
    typedef unsigned short  UWORD;
    typedef unsigned long   ULONG;
    typedef int             BOOL;
    typedef void           *APTR;
    typedef char           *STRPTR;
    typedef const char     *CONST_STRPTR;
    typedef long            LONG;
    typedef short           WORD;
    typedef void           *BPTR;
    typedef void            VOID;

    /* Boolean constants */
    #define TRUE    1
    #define FALSE   0

    /* Path and text formatting */
    #ifdef _WIN32
        #define CLI_PATH_SEP   '\\'
    #else
        #define CLI_PATH_SEP   '/'
    #endif
    #define CLI_EOL        "\n"

#endif

#endif /* PLATFORM_H */

/* End of Text */