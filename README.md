# Amiga CLI Wrapper

A collection of command-line wrapper utilities for Amiga development with dual-target architecture.

## Features

- Cross-platform CLI tools that work on both Amiga and modern systems
- Dual-target compilation: vbcc for Amiga, GCC for host development
- Platform abstraction layer for seamless cross-compilation
- ANSI C compatibility for classic Amiga hardware
- Minimal dependencies and resource usage

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