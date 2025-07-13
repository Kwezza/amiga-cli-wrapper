#ifndef CLI_WRAPPER_H
#define CLI_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief List files in an LHA archive and calculate total uncompressed size
 *
 * Executes the specified list command (e.g., "lha l archive.lha") and parses
 * the output to extract file information and calculate total size. All parsing
 * and progress is logged to logfile.txt.
 *
 * @param cmd Complete command string to execute (e.g., "lha l archive.lha")
 * @param out_total Pointer to receive total uncompressed size in bytes
 * @return true if command executed successfully and parsing completed
 * @return false if command failed or parsing errors occurred
 */
bool cli_list(const char *cmd, uint32_t *out_total);

/**
 * @brief Extract files from an LHA archive with real-time progress tracking
 *
 * Executes the specified extract command (e.g., "lha x -m -n archive.lha dest/")
 * and parses the output line-by-line to track progress. Each extracted file
 * contributes to a cumulative byte count, with percentage calculated against
 * the expected total. All parsing and progress is logged to logfile.txt.
 *
 * @param cmd Complete command string to execute (e.g., "lha x -m -n archive.lha dest/")
 * @param total_expected Total bytes expected to be extracted (from cli_list)
 * @return true if extraction completed successfully
 * @return false if extraction failed
 */
bool cli_extract(const char *cmd, uint32_t total_expected);

/**
 * @brief Initialize CLI wrapper logging system
 *
 * Sets up the debug logging system. Called automatically by cli_list/cli_extract
 * but can be called explicitly to initialize logging earlier.
 *
 * @return true if logging initialized successfully
 * @return false if logging setup failed
 */
bool cli_wrapper_init(void);

/**
 * @brief Cleanup CLI wrapper resources
 *
 * Closes log files and cleans up any resources used by the CLI wrapper.
 * Should be called at program exit.
 */
void cli_wrapper_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* CLI_WRAPPER_H */
