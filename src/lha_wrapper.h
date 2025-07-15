#ifndef LHA_WRAPPER_H
#define LHA_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LHA wrapper system
 *
 * Sets up the LHA wrapper system and underlying process control.
 * Must be called before using other LHA wrapper functions.
 *
 * @return true if initialization successful
 * @return false if initialization failed
 */
bool lha_wrapper_init(void);

/**
 * @brief Cleanup LHA wrapper system
 *
 * Cleans up LHA wrapper resources and process control system.
 * Should be called at program exit.
 */
void lha_wrapper_cleanup(void);

/**
 * @brief List files in an LHA archive using controlled process
 *
 * Executes the specified list command using the controlled process system
 * and parses the output to extract file information and calculate total size.
 * Provides full process control including signal handling and death monitoring.
 *
 * @param cmd Complete command string to execute (e.g., "lha l archive.lha")
 * @param out_total Pointer to receive total uncompressed size in bytes
 * @param out_file_count Pointer to receive number of files in archive (can be NULL)
 * @return true if command executed successfully and parsing completed
 * @return false if command failed or parsing errors occurred
 */
bool lha_controlled_list(const char *cmd, uint32_t *out_total, uint32_t *out_file_count);

/**
 * @brief Extract files from an LHA archive using controlled process
 *
 * Executes the specified extract command using the controlled process system
 * and parses the output line-by-line to track progress. Provides full process
 * control including pause/resume capabilities and death monitoring.
 *
 * @param cmd Complete command string to execute (e.g., "lha x -m -n archive.lha dest/")
 * @param total_expected Total bytes expected to be extracted (from lha_controlled_list)
 * @return true if extraction completed successfully
 * @return false if extraction failed
 */
bool lha_controlled_extract(const char *cmd, uint32_t total_expected);

#ifdef __cplusplus
}
#endif

#endif /* LHA_WRAPPER_H */
