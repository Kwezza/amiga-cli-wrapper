 Copilot Coding Instructions
*(Amiga CLI Wrapper — dual-target: Amiga / Host)*

> **Context**
> This is a streaming archive extraction wrapper with real-time progress tracking.
> • **Amiga** – Native tools using vbcc +aos68k, async pipes, and AmigaOS APIs
> • **Host**  – Development/testing stubs compiled with GCC on Windows/Linux/macOS
>
> **Core Purpose**: Wrap LHA/unzip commands with streaming output parsing for progress feedback.
> Uses `#if PLATFORM_AMIGA` guards. **No malloc/free** - uses static buffers for memory safety.

## Project Architecture

**Core Design Philosophy:**
- **Streaming Command Execution** - Real-time parsing of external tool output via pipes
- **Progress Tracking** - Byte-level progress calculation for archive operations
- **Memory Safety** - Static buffers only, no dynamic allocation for Amiga stability
- **Dual-phase Operations** - List first (get total), then extract with progress

**Key Implementation Patterns:**
- **`src/cli_wrapper.c`** - Single-file implementation with all logic
- **Amiga execution**: `SystemTagList()` + named pipes (`PIPE:prefix.taskid`)
- **Line processors**: Callback pattern for streaming output parsing
- **Static string parsing**: Fixed-size buffers with bounds checking


---

## Essential Development Workflows

### Critical Amiga Execution Pattern
```c
// Amiga async execution with streaming output
amiga_exec_config_t config = {
    .tool_name = "LhA",
    .pipe_prefix = "lha_pipe",
    .timeout_seconds = 15,
    .silent_mode = false
};
execute_command_amiga_streaming(cmd, line_processor, &ctx, &config);
```

### Memory Management Rules
- **NO malloc/free** - Use static buffers: `char buf[64]`, `static char filename[64]`
- **Buffer safety**: Always use `sizeof(buffer)` bounds checking
- **Timing**: Use `volatile int delay_counter` busy-wait, **never Delay()** (causes crashes)

### Build Commands (Critical)
| Command | Purpose | Output Location |
|---------|---------|----------------|
| `make` or `make TARGET=amiga` | Build Amiga CLI tools | `build/amiga/` |
| `make TARGET=host` | Build host tools + run tests | `build/host/` |
| `make test TARGET=host` | Run comprehensive test suite | Tests execute automatically |

### Key Files
- **Core logic**: `src/cli_wrapper.c` (single implementation file)
- **API**: `include/cli_wrapper.h` (list/extract functions for LHA/unzip)
- **Test example**: `test/cli_wrapper_test.c` (shows proper usage pattern)

## Critical Amiga Implementation Patterns

### Named Pipe Execution (Core Architecture)
```c
// 1. Generate unique pipe: PIPE:lha_pipe.{taskid}
sprintf(pipe_name, "PIPE:%s.%lu", config->pipe_prefix, (unsigned long)current_task);

// 2. Async command execution
snprintf(full_cmd, sizeof(full_cmd), "%s >%s", cmd, pipe_name);
SystemTagList(full_cmd, tags);  // Background execution

// 3. Real-time streaming read
while (empty_reads < MAX_EMPTY_READS) {
    bytesRead = Read(read_pipe, buf, sizeof(buf) - 1);
    // Process line-by-line for progress tracking
}
```

### Progress Tracking Pattern
```c
// Two-phase operation: list → extract
cli_list("lha l archive.lha", &total_size);           // Get total bytes
cli_extract("lha x archive.lha dest/", total_size);   // Track progress

// Line processor callbacks parse output in real-time
extract_context_t ctx = {total_expected, 0, 0, 0};
parse_lha_extract_line(line, &file_size, filename, sizeof(filename));
```

### Safe Timing (Critical for Amiga Stability)
```c
// NEVER use Delay() - causes system crashes
// Use busy-wait loops instead:
volatile int delay_counter;
for (delay_counter = 0; delay_counter < 200000; delay_counter++) {
    /* 400ms equivalent busy-wait */
}
```

## Language & Compiler Flags

| Target | Compiler | Key switches |
|--------|----------|-------------|
| **Amiga** | `vc` (vbcc driver) | `+aos68k -cpu=68000 -std=c99 -pedantic -Wall` |
| **Host**  | gcc         | `-std=c99 -pedantic -Wall -Wextra` |

*Both targets must build warning-free; treat warnings as errors.*

---


## Build Directory Structure

**All build artifacts must be placed in target-specific directories:**

### Directory Layout:
```
build/
├── amiga/          # Amiga CLI binaries (no .exe extension)
├── host/           # Host platform binaries (.exe on Windows)

tests/
├── shared/         # Test source code that compiles for both targets
├── amiga/          # Amiga-specific test source code
└── host/           # Host-specific test source code
```

### Binary Placement Rules:
- **Amiga binaries**: `build/amiga/` (no .exe extension)
- **Host binaries**: `build/host/` (.exe extension on Windows)
- **Test programs**: Follow same pattern as main binaries
- **Never mix**: Keep host and Amiga artifacts completely separate

---

## Character Encoding Rules

**Always use ASCII-only characters in code and output messages for Amiga compatibility:**

### Forbidden Unicode Characters:
- ❌ `…` (U+2026 horizontal ellipsis) - use `...` (three periods)
- ❌ `—` (U+2014 em dash) - use `-` (regular hyphen)
- ❌ `'` `'` (U+2018/2019 smart quotes) - use `'` (straight apostrophe)
- ❌ `"` `"` (U+201C/201D smart quotes) - use `"` (straight quotes)

### Safe ASCII Alternatives:
- ✅ Use `...` for ellipsis
- ✅ Use `-` for dashes
- ✅ Use `*` for bullets
- ✅ Use `->` for arrows
- ✅ Use straight quotes `'` and `"`

**Reason**: AmigaOS and classic systems only support ASCII/ISO-8859-1, not Unicode.

---

*Follow these rules and the output will compile cleanly under both
vbcc +aos68k and modern host compilers, with consistent style.*
