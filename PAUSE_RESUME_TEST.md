# Pause-Then-Quit Test (Standalone)

This is a standalone test program specifically designed to test the pause-then-quit functionality of the process control system at normal Amiga speeds (7MHz). This demonstrates the practical use case of pausing output during user prompts and then terminating the process.

## Purpose

This test program:
- Extracts the A10TankKiller_v2.0_3Disk.lha archive using LHA
- Monitors the extraction progress in real-time
- **Automatically pauses output** after exactly 5 files have been extracted (SIGBREAKF_CTRL_S)
- Simulates a 3-second user cancellation prompt
- **Sends quit command** to terminate the LHA process (SIGBREAKF_CTRL_C)
- **Monitors for output** after the quit signal to verify termination
- Logs all activity to `pause_resume_log.txt`

## Test Sequence

1. **Normal Extraction**: Files 1-5 extract normally with progress display
2. **Pause Output**: Send SIGBREAKF_CTRL_S to stop console output (simulating user prompt)
3. **User Decision**: 3-second countdown simulating "Cancel process? (Y/n)" prompt
4. **Quit Process**: Send SIGBREAKF_CTRL_C to terminate the LHA process
5. **Monitor**: Check if any output appears after quit signal
6. **Report**: Show whether process terminated or continued

## Expected Behavior

At normal Amiga speeds, the test should demonstrate:
- **Pause works**: Output stops during the 3-second user prompt
- **Quit works**: SIGBREAKF_CTRL_C terminates the LHA process
- **Clean termination**: No output appears after quit signal
- **Process respects signals**: LHA responds to standard Amiga break signals

## Test Results

The test will show:
- **Files processed**: Total number of files extracted before termination
- **Files at pause point**: Number of files when pause was triggered (should be 5)
- **Pause requested**: Whether pause signal was sent (should be "yes")
- **Quit requested**: Whether quit signal was sent (should be "yes")
- **Output after quit**: Whether any output appeared after quit (should be "no")
- **Lines after quit**: Number of lines received after quit (should be 0)
- **Completion detected**: Whether process terminated cleanly

## Practical Use Case

This test demonstrates the pattern for:
```
1. User presses Ctrl+C during long operation
2. Program pauses output (SIGBREAKF_CTRL_S)
3. Program shows: "Cancel operation? (Y/n)"
4. If user chooses Y: send SIGBREAKF_CTRL_C
5. If user chooses N: send SIGBREAKF_CTRL_Q to resume
```

This gives users a clean way to cancel long-running operations without sudden termination.

## Files

- **Source**: `test/pause_resume_test.c`
- **Executable**: `build/amiga/pause_resume_test`
- **Test Archive**: `assets/A10TankKiller_v2.0_3Disk.lha`
- **Log File**: `pause_resume_log.txt` (created when test runs)

## How to Run

1. **Build the test** (if not already built):
   ```
   make build-pause-resume-test
   ```

2. **Run the test** at normal Amiga speed (7MHz recommended):
   ```
   cd build/amiga
   ./pause_resume_test
   ```

3. **Watch the output** - you should see:
   - Real-time file extraction progress
   - "PAUSE POINT REACHED AFTER 5 FILES" message
   - "Pause signal sent successfully" message
   - "Pause.." followed by countdown "4.." "3.." "2.." "1.."
   - "Continuing" message
   - "Resume signal sent successfully" message
   - Continuation of extraction

4. **Check the log file** for detailed information:
   ```
   cat pause_resume_log.txt
   ```

## Expected Behavior

At normal Amiga speeds, the LHA process should be slow enough that:
- The pause signal reaches the LHA process before it completes
- The LHA process actually pauses during the countdown
- The resume signal successfully restarts the extraction
- The test demonstrates **real** pause/resume functionality

## Test Results

The test will show:
- **Files processed**: Total number of files extracted
- **Files at pause point**: Number of files extracted when pause was triggered (should be 5)
- **Pause requested**: Whether pause signal was sent (should be "yes")
- **Resume requested**: Whether resume signal was sent (should be "yes")
- **Completion detected**: Whether extraction completed successfully (should be "yes")

## Troubleshooting

If the test shows "Failed to send pause signal" or "Failed to send resume signal":
- The LHA process may be completing too quickly (try slower emulation)
- The process control system may not be working properly
- Check the detailed log in `pause_resume_log.txt`

## Differences from Main Test Suite

This standalone test is optimized for:
- **Single purpose**: Only tests pause/resume functionality
- **Real process control**: Uses actual LHA process with real signals
- **Detailed logging**: Comprehensive logging for debugging
- **Fast execution**: No other tests to slow down the process
- **7MHz friendly**: Designed to work at normal Amiga speeds

This should give you a clear picture of whether the pause/resume system is working correctly without having to run the entire test suite at slow speeds.
