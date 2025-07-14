# Amiga CLI Wrapper

A collection of command-line wrapper utilities for Amiga development with dual-target architecture.

## Features

- Cross-platform CLI tools that work on both Amiga and modern systems
- Dual-target compilation: vbcc for Amiga, GCC for host development
- Platform abstraction layer for seamless cross-compilation
- ANSI C compatibility for classic Amiga hardware
- Minimal dependencies and resource usage
- **NEW: Byte-level progress tracking for smooth extraction on slower Amiga systems**

## Archive Extraction Modes

This wrapper provides two extraction modes optimized for different Amiga systems:

### File-Level Progress (Standard)
- **Function**: `cli_extract()`
- **Best for**: Fast Amiga systems (68020+, 8MB+ RAM)
- **Display**: Shows progress per file extracted
- **Command**: `lha x -m -n archive.lha dest/`

### Byte-Level Progress (Optimized for slower systems)
- **Function**: `cli_extract_bytes()`
- **Best for**: Slower Amiga systems (68000, 2-4MB RAM)
- **Display**: Shows smooth progress based on bytes extracted
- **Command**: `lha -m -D0 -U16 x archive.lha dest/`
- **Configurable**: Update interval controlled by `LHA_UPDATE_INTERVAL_KB` (default: 16 KiB)

The byte-level mode reduces display updates and provides smoother progress feedback,
preventing system slowdowns during large file extractions on classic hardware.

## System Requirements

### Amiga Target
- Amiga with Workbench 2.0+ (3.0+ recommended)
- 68000 CPU minimum
- 2MB RAM minimum (4MB recommended)
- CLI/Shell environment

### Host Target (Development)
- Windows, Linux, or macOS
- GCC or compatible C compiler
- Make utility

## Building

This project supports dual-target compilation:

**Amiga target (default):**
```bash
make TARGET=amiga
```

**Host target (development/testing):**
```bash
make TARGET=host
```

**Or simply:**
```bash
make all
```

## Usage Examples

### Basic Archive Operations

```c
#include "cli_wrapper.h"

int main(void) {
    uint32_t total_size;

    // Initialize the wrapper
    cli_wrapper_init();

    // List archive contents
    if (cli_list("lha l archive.lha", &total_size)) {
        printf("Archive size: %lu bytes\n", total_size);

        // Standard extraction (fast systems)
        cli_extract("lha x -m -n archive.lha dest/", total_size);

        // OR byte-level extraction (slower systems)
        cli_extract_bytes("lha -m -D0 -U16 x archive.lha dest/", total_size);
    }

    cli_wrapper_cleanup();
    return 0;
}
```

### Configuration for Slower Systems

For 7 MHz Amiga systems, adjust the update interval:

```c
// In cli_wrapper.h or your build system:
#define LHA_UPDATE_INTERVAL_KB 32  // Update every 32 KiB (slower updates)

// Command will be: lha -m -D0 -U32 x archive.lha dest/
```

### Test Programs

Two test programs are provided:

- `cli_wrapper_test` - Standard file-level extraction test
- `cli_bytes_test` - Byte-level extraction test (recommended for slower systems)

## Dual-Target Platform Support

This project uses a unified source tree that builds on both:
- **Amiga target** - Compiled with vbcc +aos68k for genuine Amiga hardware
- **Host target** - Compiled with GCC on Windows/Linux/macOS for development

Amiga-specific code is isolated using `#if PLATFORM_AMIGA` preprocessor guards, while
portable modules use standard C99. Memory allocation and I/O operations use
platform abstraction wrappers for maximum compatibility.

## Project Structure

### Source Organization
- `src/core/` - Core CLI framework and utilities
- `src/platform/` - Platform abstraction layer
- `src/tools/` - Individual CLI tool implementations
- `src/utils/` - Utility functions and helpers

### Directory Layout
- `include/` - Header files (single source of truth)
- `build/` - Build outputs (amiga/host)
- `tests/` - Test cases and frameworks
- `docs/` - Documentation and specifications
- `scripts/` - Build and utility scripts

## Usage

After building, the CLI tools will be available in:
- **Amiga**: `build/amiga/`
- **Host**: `build/host/`

Each tool includes built-in help:
```bash
./tool_name --help
```

## Testing

The project includes comprehensive test suites for both platforms:

```bash
# Run host-based unit tests
make test TARGET=host

# Build Amiga tests (transfer to Amiga to run)
make test TARGET=amiga
```

## Contributing

1. Follow the coding standards in `.github/copilot-instructions.md`
2. Ensure both Amiga and host targets build without warnings
3. Add tests for new functionality
4. Update documentation as needed

## Architecture Notes

This project follows the dual-target architecture pattern:

- **Platform Abstraction**: All I/O and memory operations go through `cli_*` wrappers
- **Conditional Compilation**: Use `#if PLATFORM_AMIGA` for Amiga-specific code
- **Header Organization**: All headers in `include/` hierarchy, never in `src/`
- **Build Separation**: Amiga and host artifacts kept completely separate

## License

MIT License - see individual source files for details.

## Author

Kerry Thompson (2025)

---

*This project is designed to respect classic Amiga development practices while
enabling modern development workflows through cross-compilation.*
