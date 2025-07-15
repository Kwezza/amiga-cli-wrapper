/*
 * File Corruptor - Host-only test utility
 * Randomly corrupts 5 bytes in a file to test CRC validation
 * 
 * This tool is designed for testing archive integrity checks
 * by introducing controlled corruption into files.
 * 
 * Usage: file_corruptor <filename>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#ifdef PLATFORM_AMIGA
#error "File corruptor is host-only tool, not available on Amiga"
#endif

#define CORRUPTION_BYTE_COUNT 5
#define MAX_FILE_SIZE (10 * 1024 * 1024)  /* 10MB max file size */

/* Function prototypes */
static bool corrupt_file(const char *filename);
static long get_file_size(FILE *file);
static void print_usage(const char *program_name);
static void print_corruption_details(const char *filename, long file_size, 
                                   long positions[], uint8_t original_bytes[], 
                                   uint8_t new_bytes[]);

int main(int argc, char *argv[])
{
    printf("File Corruptor - Host Test Utility\n");
    printf("==================================\n");
    
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    
    printf("Target file: %s\n", filename);
    
    /* Initialize random seed */
    srand((unsigned int)time(NULL));
    
    /* Corrupt the file */
    if (!corrupt_file(filename)) {
        fprintf(stderr, "Error: Failed to corrupt file '%s'\n", filename);
        return 1;
    }
    
    printf("File corruption completed successfully!\n");
    printf("Note: This file should now fail CRC checks.\n");
    
    return 0;
}

static bool corrupt_file(const char *filename)
{
    if (!filename) {
        fprintf(stderr, "Error: Invalid filename\n");
        return false;
    }
    
    /* Open file for reading and writing */
    FILE *file = fopen(filename, "r+b");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", filename, strerror(errno));
        return false;
    }
    
    /* Get file size */
    long file_size = get_file_size(file);
    if (file_size <= 0) {
        fprintf(stderr, "Error: Cannot determine file size or file is empty\n");
        fclose(file);
        return false;
    }
    
    if (file_size > MAX_FILE_SIZE) {
        fprintf(stderr, "Error: File too large (%ld bytes). Maximum supported: %d bytes\n", 
                file_size, MAX_FILE_SIZE);
        fclose(file);
        return false;
    }
    
    /* Ensure file is large enough for corruption */
    if (file_size < CORRUPTION_BYTE_COUNT) {
        fprintf(stderr, "Error: File too small (%ld bytes). Need at least %d bytes\n", 
                file_size, CORRUPTION_BYTE_COUNT);
        fclose(file);
        return false;
    }
    
    printf("File size: %ld bytes\n", file_size);
    printf("Corrupting %d random bytes...\n", CORRUPTION_BYTE_COUNT);
    
    /* Generate unique random positions */
    long positions[CORRUPTION_BYTE_COUNT];
    uint8_t original_bytes[CORRUPTION_BYTE_COUNT];
    uint8_t new_bytes[CORRUPTION_BYTE_COUNT];
    
    for (int i = 0; i < CORRUPTION_BYTE_COUNT; i++) {
        bool position_unique = false;
        
        /* Find a unique position */
        while (!position_unique) {
            positions[i] = rand() % file_size;
            position_unique = true;
            
            /* Check if this position is already used */
            for (int j = 0; j < i; j++) {
                if (positions[j] == positions[i]) {
                    position_unique = false;
                    break;
                }
            }
        }
        
        /* Read original byte */
        if (fseek(file, positions[i], SEEK_SET) != 0) {
            fprintf(stderr, "Error: Cannot seek to position %ld\n", positions[i]);
            fclose(file);
            return false;
        }
        
        int byte_read = fgetc(file);
        if (byte_read == EOF) {
            fprintf(stderr, "Error: Cannot read byte at position %ld\n", positions[i]);
            fclose(file);
            return false;
        }
        
        original_bytes[i] = (uint8_t)byte_read;
        
        /* Generate a different byte */
        do {
            new_bytes[i] = (uint8_t)(rand() % 256);
        } while (new_bytes[i] == original_bytes[i]);
        
        /* Write corrupted byte */
        if (fseek(file, positions[i], SEEK_SET) != 0) {
            fprintf(stderr, "Error: Cannot seek to position %ld for writing\n", positions[i]);
            fclose(file);
            return false;
        }
        
        if (fputc(new_bytes[i], file) == EOF) {
            fprintf(stderr, "Error: Cannot write byte at position %ld\n", positions[i]);
            fclose(file);
            return false;
        }
    }
    
    /* Flush changes */
    if (fflush(file) != 0) {
        fprintf(stderr, "Error: Cannot flush file changes\n");
        fclose(file);
        return false;
    }
    
    /* Print corruption details */
    print_corruption_details(filename, file_size, positions, original_bytes, new_bytes);
    
    fclose(file);
    return true;
}

static long get_file_size(FILE *file)
{
    if (!file) {
        return -1;
    }
    
    /* Save current position */
    long current_pos = ftell(file);
    if (current_pos == -1) {
        return -1;
    }
    
    /* Seek to end */
    if (fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }
    
    /* Get file size */
    long file_size = ftell(file);
    
    /* Restore position */
    if (fseek(file, current_pos, SEEK_SET) != 0) {
        return -1;
    }
    
    return file_size;
}

static void print_usage(const char *program_name)
{
    printf("Usage: %s <filename>\n", program_name);
    printf("\n");
    printf("Corrupts exactly %d random bytes in the specified file.\n", CORRUPTION_BYTE_COUNT);
    printf("This is intended for testing archive CRC validation.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s test_archive.lha\n", program_name);
    printf("  %s corrupted_file.dat\n", program_name);
    printf("\n");
    printf("Note: This tool modifies the file in-place. Make a backup first!\n");
}

static void print_corruption_details(const char *filename, long file_size, 
                                   long positions[], uint8_t original_bytes[], 
                                   uint8_t new_bytes[])
{
    printf("\nCorruption Details:\n");
    printf("===================\n");
    printf("File: %s\n", filename);
    printf("Size: %ld bytes\n", file_size);
    printf("Bytes corrupted: %d\n", CORRUPTION_BYTE_COUNT);
    printf("\n");
    printf("Corruption map:\n");
    
    for (int i = 0; i < CORRUPTION_BYTE_COUNT; i++) {
        printf("  Position %ld: 0x%02X -> 0x%02X (decimal: %d -> %d)\n", 
               positions[i], original_bytes[i], new_bytes[i], 
               original_bytes[i], new_bytes[i]);
    }
    
    printf("\n");
}
