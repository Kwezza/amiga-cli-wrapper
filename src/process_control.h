#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef PLATFORM_AMIGA
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process control structure for managing child processes
 */
typedef struct {
#ifdef PLATFORM_AMIGA
    struct Process *child_process;    /* Process handle for signals */
    BPTR input_pipe;                  /* To send commands to child */
    BPTR output_pipe;                 /* To receive output from child */
    ULONG death_signal;               /* Signal mask for death notification */
#else
    void *child_process;              /* Host stub */
    void *input_pipe;                 /* Host stub */
    void *output_pipe;                /* Host stub */
    uint32_t death_signal;            /* Host stub */
#endif
    bool process_running;             /* Current status flag */
    char process_name[32];            /* For debugging */
} controlled_process_t;

/**
 * @brief Configuration for process execution
 */
typedef struct {
    const char *tool_name;            /* Name of the tool (e.g., "LhA") */
    const char *pipe_prefix;          /* Prefix for pipe names */
    uint32_t timeout_seconds;         /* Timeout for process operations */
    bool silent_mode;                 /* Suppress output to console */
} process_exec_config_t;

/**
 * @brief Execute a command with full process control
 *
 * Creates a new controlled process using CreateNewProc() and executes the
 * specified command. Returns a process handle for signal management and
 * provides bidirectional communication via pipes.
 *
 * @param cmd Complete command string to execute
 * @param line_processor Callback function to process output lines
 * @param user_data User data passed to line processor
 * @param config Process execution configuration
 * @param out_process Pointer to receive process control structure
 * @return true if process created and command executed successfully
 * @return false if process creation or execution failed
 */
bool execute_controlled_process(const char *cmd,
                                bool (*line_processor)(const char *, void *),
                                void *user_data,
                                const process_exec_config_t *config,
                                controlled_process_t *out_process);

/**
 * @brief Send pause signal to controlled process
 *
 * @param process Process control structure
 * @return true if signal sent successfully
 * @return false if signal failed or process not running
 */
bool send_pause_signal(controlled_process_t *process);

/**
 * @brief Send resume signal to controlled process
 *
 * @param process Process control structure
 * @return true if signal sent successfully
 * @return false if signal failed or process not running
 */
bool send_resume_signal(controlled_process_t *process);

/**
 * @brief Send terminate signal to controlled process
 *
 * @param process Process control structure
 * @return true if signal sent successfully
 * @return false if signal failed or process not running
 */
bool send_terminate_signal(controlled_process_t *process);

/**
 * @brief Wait for process death signal
 *
 * @param process Process control structure
 * @param timeout_seconds Maximum time to wait (0 = no timeout)
 * @return true if process death detected
 * @return false if timeout or error occurred
 */
bool wait_for_death_signal(controlled_process_t *process, uint32_t timeout_seconds);

/**
 * @brief Force kill process (emergency termination)
 *
 * @param process Process control structure
 * @return true if process killed successfully
 * @return false if kill failed
 */
bool force_kill_process(controlled_process_t *process);

/**
 * @brief Clean up process control resources
 *
 * Closes pipes and frees any resources associated with the process.
 * Should be called when process is no longer needed.
 *
 * @param process Process control structure to clean up
 */
void cleanup_controlled_process(controlled_process_t *process);

/**
 * @brief Initialize process control system
 *
 * Sets up the process control system for the current task.
 * Must be called before using other process control functions.
 *
 * @return true if initialization successful
 * @return false if initialization failed
 */
bool process_control_init(void);

/**
 * @brief Cleanup process control system
 *
 * Cleans up global process control resources.
 * Should be called at program exit.
 */
void process_control_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* PROCESS_CONTROL_H */
