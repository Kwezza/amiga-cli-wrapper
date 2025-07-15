#include "process_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef PLATFORM_AMIGA
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <utility/tagitem.h>
#include <proto/dos.h>
#include <proto/exec.h>

/* Define additional signal constants not in all headers */
#ifndef SIGBREAKF_CTRL_S
#define SIGBREAKF_CTRL_S    (1L<<19)  /* Pause signal (Ctrl+S) */
#endif
#ifndef SIGBREAKF_CTRL_Q
#define SIGBREAKF_CTRL_Q    (1L<<17)  /* Resume signal (Ctrl+Q) */
#endif

#else
/* Define Amiga signal constants for compilation */
#define SIGBREAKF_CTRL_C    (1L<<12)
#define SIGBREAKF_CTRL_D    (1L<<13)
#define SIGBREAKF_CTRL_E    (1L<<14)
#define SIGBREAKF_CTRL_F    (1L<<15)
#define SIGBREAKF_CTRL_S    (1L<<19)  /* Pause signal */
#define SIGBREAKF_CTRL_Q    (1L<<17)  /* Resume signal */
#define BNULL               0L
#define TRUE                1
#define FALSE               0
#define TAG_END             0L
#define SYS_Input           1L
#define SYS_Output          2L
#define SYS_Error           3L
#define SYS_Asynch          4L
#define MODE_OLDFILE        1005L
#define MODE_NEWFILE        1006L
typedef void *BPTR;
typedef long LONG;
typedef struct Task *task_ptr;
typedef struct TagItem { unsigned long ti_Tag; void *ti_Data; } TagItem;

/* Stub functions for non-Amiga compilation */
static BPTR Open(const char *name, long mode) { return (BPTR)1; }
static void Close(BPTR file) { }
static LONG Read(BPTR file, void *buffer, long length) { return 0; }
static long SystemTagList(const char *command, struct TagItem *tags) { return 0; }
static struct Task *FindTask(struct Task *task) { return (struct Task *)1; }
static void Signal(struct Task *task, unsigned long signals) { }
static unsigned long Wait(unsigned long signals) { return signals; }
#endif

/* Global state */
static bool g_process_control_initialized = false;
static FILE *g_process_logfile = NULL;

/* Internal helper functions */
static void process_log_message(const char *format, ...);
static void process_log_timestamp(void);

static bool create_process_pipes(const char *pipe_prefix, BPTR *input_pipe, BPTR *output_pipe, char *pipe_name, size_t pipe_name_size);
static bool spawn_amiga_process(const char *cmd, const char *pipe_name, controlled_process_t *process);
static bool read_process_output(controlled_process_t *process, bool (*line_processor)(const char *, void *), void *user_data);
static void cleanup_amiga_process(controlled_process_t *process);

bool process_control_init(void)
{
    if (g_process_control_initialized) {
        return true;
    }

    /* Try to open process log file */
    g_process_logfile = fopen("logfile.txt", "a");
    if (!g_process_logfile) {
#ifdef PLATFORM_AMIGA
        g_process_logfile = fopen("T:logfile.txt", "a");
        if (!g_process_logfile) {
            g_process_logfile = fopen("RAM:logfile.txt", "a");
        }
#endif
    }

    g_process_control_initialized = true;

    if (g_process_logfile) {
        process_log_message("=== Process Control System Initialized ===");
        process_log_message("Platform: Amiga");
    }

    return true;
}

void process_control_cleanup(void)
{
    if (g_process_logfile) {
        process_log_message("=== Process Control System Cleanup ===");
        fclose(g_process_logfile);
        g_process_logfile = NULL;
    }
    g_process_control_initialized = false;
}

bool execute_controlled_process(const char *cmd,
                                bool (*line_processor)(const char *, void *),
                                void *user_data,
                                const process_exec_config_t *config,
                                controlled_process_t *out_process)
{
    if (!cmd || !config || !out_process) {
        return false;
    }

    if (!g_process_control_initialized) {
        if (!process_control_init()) {
            return false;
        }
    }

    /* Initialize process structure */
    {
        int i;
        char *ptr = (char *)out_process;
        for (i = 0; i < sizeof(controlled_process_t); i++) {
            ptr[i] = 0;
        }
    }
    
    /* Copy process name safely */
    {
        int i;
        const char *src = config->tool_name;
        char *dst = out_process->process_name;
        for (i = 0; i < sizeof(out_process->process_name) - 1 && src[i] != '\0'; i++) {
            dst[i] = src[i];
        }
        dst[i] = '\0';
    }

    process_log_message("Starting controlled process: %s", config->tool_name);
    process_log_message("Command: %s", cmd);

    char pipe_name[64];
    
    /* Create communication pipes */
    if (!create_process_pipes(config->pipe_prefix, &out_process->input_pipe, 
                             &out_process->output_pipe, pipe_name, sizeof(pipe_name))) {
        process_log_message("Failed to create process pipes");
        return false;
    }

    /* Spawn the process */
    if (!spawn_amiga_process(cmd, pipe_name, out_process)) {
        process_log_message("Failed to spawn Amiga process");
        cleanup_amiga_process(out_process);
        return false;
    }

    /* Read output and process lines */
    bool result = read_process_output(out_process, line_processor, user_data);
    
    /* For now, we'll attempt to capture exit code by re-running synchronously */
    /* This is a temporary solution - in Phase 2 we'll use proper process monitoring */
    if (result) {
        /* Re-execute synchronously to get exit code */
        char sync_cmd[512];
        snprintf(sync_cmd, sizeof(sync_cmd), "%s >NIL:", cmd);
        
        process_log_message("Re-executing command synchronously to get exit code: %s", sync_cmd);
        
        struct TagItem sync_tags[] = {
            {SYS_Input, 0},
            {SYS_Output, 0},
            {SYS_Error, 0},
            {SYS_Asynch, FALSE},  /* Synchronous execution */
            {TAG_END, 0}
        };
        
        LONG exit_code = SystemTagList(sync_cmd, sync_tags);
        out_process->exit_code = exit_code;
        out_process->exit_code_valid = true;
        
        process_log_message("Command exit code: %ld", exit_code);
        
        /* If exit code is non-zero, consider it a warning but not a failure */
        /* LHA returns non-zero codes for warnings (like file creation errors) */
        if (exit_code != 0) {
            process_log_message("Warning: Process completed with non-zero exit code: %ld", exit_code);
        }
    }
    
    process_log_message("Process completed with result: %s", result ? "success" : "failure");
    
    return result;
}

bool send_pause_signal(controlled_process_t *process)
{
    if (!process || !process->process_running) {
        return false;
    }

    process_log_message("Pause signal requested for process: %s", process->process_name);

    if (process->child_process) {
        /* Send SIGBREAKF_CTRL_S signal to pause process */
        Signal((struct Task *)process->child_process, SIGBREAKF_CTRL_S);
        process_log_message("Pause signal sent to process");
        return true;
    }

    process_log_message("Pause signal failed - no child process");
    return false;
}

bool send_resume_signal(controlled_process_t *process)
{
    if (!process || !process->process_running) {
        return false;
    }

    process_log_message("Resume signal requested for process: %s", process->process_name);

    if (process->child_process) {
        /* Send SIGBREAKF_CTRL_Q signal to resume process */
        Signal((struct Task *)process->child_process, SIGBREAKF_CTRL_Q);
        process_log_message("Resume signal sent to process");
        return true;
    }

    process_log_message("Resume signal failed - no child process");
    return false;
}

bool send_terminate_signal(controlled_process_t *process)
{
    if (!process || !process->process_running) {
        return false;
    }

    process_log_message("Terminate signal requested for process: %s", process->process_name);

    if (process->child_process) {
        /* Send SIGBREAKF_CTRL_C signal to terminate process */
        Signal((struct Task *)process->child_process, SIGBREAKF_CTRL_C);
        process_log_message("Terminate signal sent to process");
        return true;
    }

    process_log_message("Terminate signal failed - no child process");
    return false;
}

bool wait_for_death_signal(controlled_process_t *process, uint32_t timeout_seconds)
{
    if (!process) {
        return false;
    }

    process_log_message("Waiting for death signal from process: %s", process->process_name);

    if (process->death_signal) {
        unsigned long signals_received;
        
        if (timeout_seconds > 0) {
            /* Wait with timeout */
            signals_received = Wait(process->death_signal | SIGBREAKF_CTRL_C);
        } else {
            /* Wait indefinitely */
            signals_received = Wait(process->death_signal);
        }
        
        if (signals_received & process->death_signal) {
            process_log_message("Death signal received from process");
            process->process_running = false;
            return true;
        }
    }

    process_log_message("Death signal wait failed - no death signal set");
    return false;
}

bool force_kill_process(controlled_process_t *process)
{
    if (!process || !process->process_running) {
        return false;
    }

    process_log_message("Force kill requested for process: %s", process->process_name);

    if (process->child_process) {
        /* Send SIGBREAKF_CTRL_F signal for emergency termination */
        Signal((struct Task *)process->child_process, SIGBREAKF_CTRL_F);
        process_log_message("Force kill signal sent to process");
        process->process_running = false;
        return true;
    }

    process_log_message("Force kill failed - no child process");
    return false;
}

void cleanup_controlled_process(controlled_process_t *process)
{
    if (!process) {
        return;
    }

    process_log_message("Cleaning up controlled process: %s", process->process_name);

    cleanup_amiga_process(process);

    /* Clear the structure */
    {
        int i;
        char *ptr = (char *)process;
        for (i = 0; i < sizeof(controlled_process_t); i++) {
            ptr[i] = 0;
        }
    }
}

bool get_process_exit_code(const controlled_process_t *process, int32_t *out_exit_code)
{
    if (!process || !out_exit_code) {
        return false;
    }

    if (!process->exit_code_valid) {
        process_log_message("Exit code not available for process: %s", process->process_name);
        return false;
    }

    *out_exit_code = (int32_t)process->exit_code;
    process_log_message("Retrieved exit code %ld for process: %s", 
                       (long)process->exit_code, process->process_name);
    return true;
}

/* Internal helper functions */

static void process_log_message(const char *format, ...)
{
    if (!g_process_logfile) {
        return;
    }

    process_log_timestamp();
    
    va_list args;
    va_start(args, format);
    vfprintf(g_process_logfile, format, args);
    va_end(args);
    
    fprintf(g_process_logfile, "\n");
    fflush(g_process_logfile);
}

static void process_log_timestamp(void)
{
    if (!g_process_logfile) {
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    fprintf(g_process_logfile, "[%02d:%02d:%02d] PROC: ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

#ifdef PLATFORM_AMIGA

static bool create_process_pipes(const char *pipe_prefix, BPTR *input_pipe, BPTR *output_pipe, 
                                char *pipe_name, size_t pipe_name_size)
{
    struct Task *current_task = FindTask(NULL);
    static uint32_t sequence_counter = 0;
    
    /* Generate unique pipe name */
    snprintf(pipe_name, pipe_name_size, "PIPE:%s.%lu.%lu", 
             pipe_prefix, (unsigned long)current_task, (unsigned long)++sequence_counter);
    
    process_log_message("Creating pipes with name: %s", pipe_name);
    
    /* Create output pipe (child writes, parent reads) */
    *output_pipe = Open(pipe_name, MODE_OLDFILE);
    if (!*output_pipe) {
        process_log_message("Failed to create output pipe");
        return false;
    }
    
    /* Create input pipe (parent writes, child reads) */
    char input_pipe_name[64];
    snprintf(input_pipe_name, sizeof(input_pipe_name), "%s.in", pipe_name);
    *input_pipe = Open(input_pipe_name, MODE_NEWFILE);
    if (!*input_pipe) {
        process_log_message("Failed to create input pipe");
        Close(*output_pipe);
        *output_pipe = 0;
        return false;
    }
    
    process_log_message("Pipes created successfully");
    return true;
}

static bool spawn_amiga_process(const char *cmd, const char *pipe_name, controlled_process_t *process)
{
    char full_cmd[512];
    
    /* Build command with output redirection */
    snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);
    
    process_log_message("Spawning process with command: %s", full_cmd);
    
    /* Initialize exit code fields */
    process->exit_code = 0;
    process->exit_code_valid = false;
    
#ifdef PLATFORM_AMIGA
    /* Try a different approach - use RunCommand instead of SystemTagList */
    /* This might give us better process control */
    
    /* Get current process for reference */
    struct Process *current_process = (struct Process *)FindTask(NULL);
    process_log_message("Current process: %p", current_process);
    
    /* Create process tags for SystemTagList */
    struct TagItem tags[] = {
        {SYS_Input, 0},
        {SYS_Output, 0},
        {SYS_Error, 0},
        {SYS_Asynch, TRUE},  /* Keep async for now - we'll enhance this in next phase */
        {TAG_END, 0}
    };
    
    /* Execute command asynchronously */
    LONG result = SystemTagList(full_cmd, tags);
    if (result != 0) {
        process_log_message("Failed to execute command, immediate error: %ld", result);
        process->exit_code = result;
        process->exit_code_valid = true;
        return false;
    }
    
    /* Wait a bit longer for process to start */
    volatile int delay_counter;
    for (delay_counter = 0; delay_counter < 500000; delay_counter++) {
        /* 1 second delay to let process start */
    }
    
    process_log_message("Attempting to find LHA child process...");
    
    /* Try different possible task names with more attempts */
    const char *possible_names[] = {"lha", "LhA", "LHA", "Lha", "CLI", "Background CLI", NULL};
    int name_index = 0;
    int attempts = 0;
    
    while (possible_names[name_index] && !process->child_process && attempts < 10) {
        process_log_message("Attempt %d: Trying to find task: %s", attempts + 1, possible_names[name_index]);
        process->child_process = (struct Process *)FindTask(possible_names[name_index]);
        
        if (!process->child_process) {
            name_index++;
            if (!possible_names[name_index]) {
                name_index = 0;  /* Start over */
                attempts++;
                
                /* Small delay between attempts */
                volatile int small_delay;
                for (small_delay = 0; small_delay < 100000; small_delay++) {
                    /* 200ms delay */
                }
            }
        }
    }
    
    if (process->child_process) {
        process_log_message("Child process found: %p (name: %s, attempts: %d)", 
                           process->child_process, possible_names[name_index], attempts + 1);
    } else {
        process_log_message("Warning: Could not find child process by any name after %d attempts", attempts);
        
        /* Try one more time with NULL (current task) as a test */
        struct Task *current = FindTask(NULL);
        process_log_message("Current task for reference: %p", current);
        
        /* As a fallback, try to find ANY task that's not the current one */
        /* This is a bit of a hack but might work for testing */
        struct Task *first_task = (struct Task *)((struct ExecBase *)SysBase)->TaskReady.lh_Head;
        process_log_message("First task in ready queue: %p", first_task);
        
        if (first_task && first_task != current) {
            process_log_message("Using first non-current task as fallback: %p", first_task);
            process->child_process = (struct Process *)first_task;
        }
    }
#endif
    
    process->process_running = true;
    process->death_signal = SIGBREAKF_CTRL_F;  /* Use CTRL+F as death signal */
    
    process_log_message("Process spawned successfully");
    return true;
}

static bool read_process_output(controlled_process_t *process, 
                               bool (*line_processor)(const char *, void *), 
                               void *user_data)
{
    if (!process || !process->output_pipe) {
        return false;
    }
    
    char buf[128];
    char line[256];
    static char line_buffer[512];
    static size_t line_pos = 0;
    
    int empty_reads = 0;
    const int MAX_EMPTY_READS = 50;
    bool result = true;
    
    process_log_message("Starting to read process output");
    
    while (process->process_running && empty_reads < MAX_EMPTY_READS) {
        LONG bytes_read = Read(process->output_pipe, buf, sizeof(buf) - 1);
        
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            empty_reads = 0;
            
            /* Process character by character to handle line breaks */
            for (int i = 0; i < bytes_read; i++) {
                char c = buf[i];
                
                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buffer[line_pos] = '\0';
                        
                        /* Copy to local buffer and process */
                        strncpy(line, line_buffer, sizeof(line) - 1);
                        line[sizeof(line) - 1] = '\0';
                        
                        if (line_processor && !line_processor(line, user_data)) {
                            result = false;
                            break;
                        }
                        
                        line_pos = 0;
                    }
                } else if (line_pos < sizeof(line_buffer) - 1) {
                    line_buffer[line_pos++] = c;
                }
            }
        } else if (bytes_read == 0) {
            empty_reads++;
            
            /* Small delay to prevent busy waiting */
            volatile int delay_counter;
            for (delay_counter = 0; delay_counter < 100000; delay_counter++) {
                /* 200ms equivalent busy-wait */
            }
        } else {
            process_log_message("Error reading from process output pipe");
            result = false;
            break;
        }
    }
    
    /* Process any remaining data in line buffer */
    if (line_pos > 0) {
        line_buffer[line_pos] = '\0';
        strncpy(line, line_buffer, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        
        if (line_processor) {
            line_processor(line, user_data);
        }
    }
    
    process_log_message("Finished reading process output, result: %s", result ? "success" : "failure");
    return result;
}

static void cleanup_amiga_process(controlled_process_t *process)
{
    if (!process) {
        return;
    }
    
    process_log_message("Cleaning up Amiga process resources");
    
    /* Close pipes */
    if (process->input_pipe) {
        Close(process->input_pipe);
        process->input_pipe = 0;
    }
    
    if (process->output_pipe) {
        Close(process->output_pipe);
        process->output_pipe = 0;
    }
    
    /* Mark process as not running */
    process->process_running = false;
    process->child_process = NULL;
    
    process_log_message("Amiga process cleanup completed");
}

#endif
