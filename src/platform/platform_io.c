/*------------------------------------------------------------------------*/
/*                                                                        *
 * platform_io.c — Platform-specific I/O implementations                 *
 * Amiga CLI Wrapper - Cross-platform file operations                    *
 *                                                                        *
 * Copyright © 2025 Kerry Thompson                                        *
 * SPDX-License-Identifier: MIT                                           *
 *                                                                        */
/*------------------------------------------------------------------------*/

#include <platform.h>

#ifdef PLATFORM_AMIGA

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>

/*------------------------------------------------------------------------*/
/**
 * @brief Check file/directory access (Amiga implementation)
 *
 * @param path File or directory path
 * @param mode Access mode (F_OK, R_OK, W_OK, X_OK)
 * @return int 0 on success, -1 on error
 */
/*------------------------------------------------------------------------*/
int cli_access(const char *path, int mode)
{
    BPTR lock;          /* File lock */
    
    /* Try to obtain a shared lock on the file/directory */
    lock = Lock((STRPTR)path, ACCESS_READ);
    if (lock == 0)
    {
        return -1;      /* File/directory does not exist or cannot be accessed */
    } /* if */
    
    UnLock(lock);
    return 0;           /* Success */
} /* cli_access */

/*------------------------------------------------------------------------*/
/**
 * @brief Create directory (Amiga implementation)
 *
 * @param path Directory path to create
 * @return int 0 on success, -1 on error
 */
/*------------------------------------------------------------------------*/
int cli_mkdir(const char *path)
{
    BPTR lock;          /* Directory lock */
    
    /* Try to create the directory */
    lock = CreateDir((STRPTR)path);
    if (lock == 0)
    {
        return -1;      /* Failed to create directory */
    } /* if */
    
    UnLock(lock);
    return 0;           /* Success */
} /* cli_mkdir */

/*------------------------------------------------------------------------*/
/* Directory scanning implementation for Amiga                           */
/*------------------------------------------------------------------------*/

struct cli_dir {
    BPTR lock;          /* Directory lock */
    struct FileInfoBlock fib;  /* File info block */
    int first_call;     /* Flag for first Examine call */
};

/*------------------------------------------------------------------------*/
/**
 * @brief Open directory for scanning (Amiga implementation)
 *
 * @param path Directory path
 * @return cli_dir_t* Directory handle or NULL on error
 */
/*------------------------------------------------------------------------*/
cli_dir_t *cli_opendir(const char *path)
{
    cli_dir_t *dir;     /* Directory handle */
    
    /* Allocate directory structure */
    dir = (cli_dir_t *)cli_malloc(sizeof(cli_dir_t));
    if (dir == NULL)
    {
        return NULL;
    } /* if */
    
    /* Obtain lock on directory */
    dir->lock = Lock((STRPTR)path, ACCESS_READ);
    if (dir->lock == 0)
    {
        cli_free(dir);
        return NULL;
    } /* if */
    
    dir->first_call = 1;
    return dir;
} /* cli_opendir */

/*------------------------------------------------------------------------*/
/**
 * @brief Read next directory entry (Amiga implementation)
 *
 * @param dir Directory handle
 * @param entry Entry structure to fill
 * @return int 1 if entry read, 0 if no more entries, -1 on error
 */
/*------------------------------------------------------------------------*/
int cli_readdir(cli_dir_t *dir, cli_dir_entry_t *entry)
{
    BOOL result;        /* DOS operation result */
    
    if (dir == NULL || entry == NULL)
    {
        return -1;
    } /* if */
    
    /* First call uses Examine, subsequent calls use ExNext */
    if (dir->first_call)
    {
        result = Examine(dir->lock, &dir->fib);
        dir->first_call = 0;
        
        /* Skip the directory entry itself */
        if (result && dir->fib.fib_DirEntryType > 0)
        {
            result = ExNext(dir->lock, &dir->fib);
        } /* if */
    } /* if */
    else
    {
        result = ExNext(dir->lock, &dir->fib);
    } /* else */
    
    if (!result)
    {
        return 0;       /* No more entries */
    } /* if */
    
    /* Copy entry information */
    strncpy(entry->name, dir->fib.fib_FileName, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->is_directory = (dir->fib.fib_DirEntryType > 0) ? 1 : 0;
    
    return 1;           /* Entry read successfully */
} /* cli_readdir */

/*------------------------------------------------------------------------*/
/**
 * @brief Close directory (Amiga implementation)
 *
 * @param dir Directory handle
 */
/*------------------------------------------------------------------------*/
void cli_closedir(cli_dir_t *dir)
{
    if (dir != NULL)
    {
        if (dir->lock != 0)
        {
            UnLock(dir->lock);
        } /* if */
        cli_free(dir);
    } /* if */
} /* cli_closedir */

#else /* PLATFORM_HOST */

/*------------------------------------------------------------------------*/
/* Host platform implementations                                         */
/*------------------------------------------------------------------------*/

#include <dirent.h>
#include <sys/stat.h>

/*------------------------------------------------------------------------*/
/**
 * @brief Read next directory entry (Host implementation)
 *
 * @param dir Directory handle (cast from DIR*)
 * @param entry Entry structure to fill
 * @return int 1 if entry read, 0 if no more entries, -1 on error
 */
/*------------------------------------------------------------------------*/
int cli_readdir(cli_dir_t *dir, cli_dir_entry_t *entry)
{
    DIR *d = (DIR *)dir;    /* Cast to DIR* */
    struct dirent *ent;     /* Directory entry */
    struct stat st;         /* File status */
    char full_path[512];    /* Full path for stat */
    
    if (d == NULL || entry == NULL)
    {
        return -1;
    } /* if */
    
    ent = readdir(d);
    if (ent == NULL)
    {
        return 0;           /* No more entries */
    } /* if */
    
    /* Copy entry name */
    strncpy(entry->name, ent->d_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    
    /* Determine if it's a directory */
    /* Note: This is a simplified implementation - full path construction
     * would require keeping track of the original directory path */
    entry->is_directory = (ent->d_type == DT_DIR) ? 1 : 0;
    
    return 1;               /* Entry read successfully */
} /* cli_readdir */

#endif /* PLATFORM_AMIGA */

/* End of Text */