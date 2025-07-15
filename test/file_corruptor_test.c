/*
 * File Corruptor Test - Demonstrates corruption detection
 * Tests the file_corruptor utility by creating a corrupted archive
 * and verifying that CRC checks fail.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifndef PLATFORM_AMIGA
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#endif

#define TEST_ARCHIVE_ORIGINAL "assets/A10TankKiller_v2.0_3Disk.lha"
#define TEST_ARCHIVE_COPY "test_archive_copy.lha"
#define TEST_ARCHIVE_CORRUPTED "test_archive_corrupted.lha"

/* Function prototypes */
static bool file_exists(const char *filename);
static bool copy_file(const char *source, const char *dest);
static int run_command(const char *command);
static void print_test_header(const char *test_name);
static void print_test_result(const char *test_name, bool passed);

int main(void)
{
    printf("=== File Corruptor Test Suite ===\n");
    printf("Testing archive corruption and CRC validation\n");
    printf("===============================================\n\n");
    
    /* Test 1: Verify original archive exists */
    print_test_header("Check original archive exists");
    if (!file_exists(TEST_ARCHIVE_ORIGINAL)) {
        printf("ERROR: Original test archive not found: %s\n", TEST_ARCHIVE_ORIGINAL);
        return 1;
    }
    print_test_result("Original archive exists", true);
    
    /* Test 2: Copy original archive */
    print_test_header("Copy original archive");
    if (!copy_file(TEST_ARCHIVE_ORIGINAL, TEST_ARCHIVE_COPY)) {
        printf("ERROR: Failed to copy original archive\n");
        return 1;
    }
    print_test_result("Archive copy created", true);
    
    /* Test 3: Verify original archive integrity */
    print_test_header("Test original archive integrity");
    printf("Running: lha t %s\n", TEST_ARCHIVE_COPY);
    int original_result = run_command("lha t " TEST_ARCHIVE_COPY);
    print_test_result("Original archive integrity", original_result == 0);
    
    /* Test 4: Create corrupted copy */
    print_test_header("Create corrupted copy");
    if (!copy_file(TEST_ARCHIVE_COPY, TEST_ARCHIVE_CORRUPTED)) {
        printf("ERROR: Failed to create corrupted copy\n");
        return 1;
    }
    print_test_result("Corrupted copy created", true);
    
    /* Test 5: Corrupt the archive */
    print_test_header("Corrupt the archive");
    printf("Running: ./file_corruptor.exe %s\n", TEST_ARCHIVE_CORRUPTED);
    int corruptor_result = run_command("./file_corruptor.exe " TEST_ARCHIVE_CORRUPTED);
    print_test_result("Archive corruption", corruptor_result == 0);
    
    if (corruptor_result != 0) {
        printf("ERROR: File corruptor failed\n");
        return 1;
    }
    
    /* Test 6: Verify corrupted archive fails integrity check */
    print_test_header("Test corrupted archive integrity");
    printf("Running: lha t %s\n", TEST_ARCHIVE_CORRUPTED);
    int corrupted_result = run_command("lha t " TEST_ARCHIVE_CORRUPTED);
    print_test_result("Corrupted archive should fail", corrupted_result != 0);
    
    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Original archive: %s (should pass integrity check)\n", 
           original_result == 0 ? "PASSED" : "FAILED");
    printf("Corrupted archive: %s (should fail integrity check)\n", 
           corrupted_result != 0 ? "FAILED as expected" : "UNEXPECTEDLY PASSED");
    
    if (original_result == 0 && corrupted_result != 0) {
        printf("\nSUCCESS: File corruptor working correctly!\n");
        printf("- Original archive passes CRC checks\n");
        printf("- Corrupted archive fails CRC checks\n");
        return 0;
    } else {
        printf("\nFAILURE: Test results unexpected\n");
        return 1;
    }
}

static bool file_exists(const char *filename)
{
    if (!filename) {
        return false;
    }
    
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

static bool copy_file(const char *source, const char *dest)
{
    if (!source || !dest) {
        return false;
    }
    
    FILE *src = fopen(source, "rb");
    if (!src) {
        printf("ERROR: Cannot open source file: %s\n", source);
        return false;
    }
    
    FILE *dst = fopen(dest, "wb");
    if (!dst) {
        printf("ERROR: Cannot create destination file: %s\n", dest);
        fclose(src);
        return false;
    }
    
    char buffer[8192];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, dst);
        if (bytes_written != bytes_read) {
            printf("ERROR: Write failed during file copy\n");
            fclose(src);
            fclose(dst);
            return false;
        }
    }
    
    fclose(src);
    fclose(dst);
    return true;
}

static int run_command(const char *command)
{
    if (!command) {
        return -1;
    }
    
    printf("Executing: %s\n", command);
    
#ifdef _WIN32
    /* Windows implementation */
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD exit_code;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    /* Create a mutable copy of the command */
    char cmd_copy[1024];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    if (!CreateProcess(NULL, cmd_copy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("ERROR: CreateProcess failed\n");
        return -1;
    }
    
    /* Wait for the process to complete */
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    /* Get exit code */
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        printf("ERROR: GetExitCodeProcess failed\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return (int)exit_code;
#else
    /* Unix/Linux implementation */
    int result = system(command);
    return WEXITSTATUS(result);
#endif
}

static void print_test_header(const char *test_name)
{
    printf("\n--- %s ---\n", test_name);
}

static void print_test_result(const char *test_name, bool passed)
{
    printf("Result: %s %s\n", test_name, passed ? "PASSED" : "FAILED");
}
