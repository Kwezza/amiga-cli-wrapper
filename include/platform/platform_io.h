/*------------------------------------------------------------------------*/
/*                                                                        *
 * platform_io.h — I/O abstraction layer                                 *
 * Amiga CLI Wrapper - Cross-platform file operations                    *
 *                                                                        *
 * Copyright © 2025 Kerry Thompson                                        *
 * SPDX-License-Identifier: MIT                                           *
 *                                                                        */
/*------------------------------------------------------------------------*/

#ifndef PLATFORM_IO_H
#define PLATFORM_IO_H

#include <stdio.h>

#ifdef PLATFORM_AMIGA
  /* Amiga I/O operations */
  #define cli_fopen   fopen
  #define cli_fclose  fclose
  #define cli_fread   fread
  #define cli_fwrite  fwrite
  #define cli_fseek   fseek
  #define cli_ftell   ftell
  #define cli_remove  remove
  #define cli_rename  rename

  /* Amiga directory operations - implemented in platform_io.c */
  int cli_access(const char *path, int mode);
  int cli_mkdir(const char *path);
#else
  /* Host I/O operations */
  #include <unistd.h>
  #define cli_fopen   fopen
  #define cli_fclose  fclose
  #define cli_fread   fread
  #define cli_fwrite  fwrite
  #define cli_fseek   fseek
  #define cli_ftell   ftell
  #define cli_remove  remove
  #define cli_rename  rename
  #define cli_access  access

  #ifdef _WIN32
    #include <direct.h>
    #define cli_mkdir(path) _mkdir(path)
  #else
    #include <sys/stat.h>
    #define cli_mkdir(path) mkdir(path, 0755)
  #endif
#endif

/* Directory scanning abstraction */
typedef struct cli_dir_entry {
    char name[256];        /* Entry name */
    int is_directory;      /* 1 if directory, 0 if file */
} cli_dir_entry_t;

typedef struct cli_dir cli_dir_t;

#ifdef PLATFORM_AMIGA
  /* Amiga directory scanning */
  cli_dir_t *cli_opendir(const char *path);
  int cli_readdir(cli_dir_t *dir, cli_dir_entry_t *entry);
  void cli_closedir(cli_dir_t *dir);
#else
  /* Host directory scanning */
  #include <dirent.h>
  #define cli_opendir(path)       ((cli_dir_t*)opendir(path))
  #define cli_closedir(dir)       closedir((DIR*)dir)
  
  /* Host readdir implementation */
  int cli_readdir(cli_dir_t *dir, cli_dir_entry_t *entry);
#endif

#endif /* PLATFORM_IO_H */

/* End of Text */