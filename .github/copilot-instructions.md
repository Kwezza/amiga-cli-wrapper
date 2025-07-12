# Copilot Coding Instructions
*(Amiga CLI Wrapper — dual-target: Amiga / Host)*

> **Context**
> This is a command-line wrapper utility library with dual-target architecture:
> • **Amiga** – Native CLI tools compiled with **vbcc +aos68k**
> • **Host**  – Development/testing tools compiled with **GCC** on Windows/Linux/macOS
>
> The project uses a **unified source tree** with platform guards `#if PLATFORM_AMIGA` to isolate
> Amiga-specific APIs. **Portable modules use standard C99 only.**
> All memory allocation uses `malloc`/`free` through platform abstraction wrappers.

## Project Architecture

**Core Design Philosophy:**
- **Single codebase, dual builds** - Shared business logic, platform-specific adapters
- **Platform abstraction** - All I/O goes through platform wrappers
- **Modular CLI tools** - Each tool is self-contained with minimal dependencies
- **ANSI C compatibility** - Targets classic Amiga hardware (68000, minimal RAM)

**Key Components:**
- **`src/core/`** - Core CLI framework and utilities
- **`src/platform/`** - Platform abstraction layer
- **`src/tools/`** - Individual CLI tool implementations
- **`include/platform/`** - Platform-specific headers and definitions

---

## Essential Development Workflows

### Build Commands (Critical)
| Command | Purpose | Output Location |
|---------|---------|----------------|
| `make` or `make TARGET=amiga` | Build Amiga CLI tools | `build/amiga/` |
| `make TARGET=host` | Build host tools + run tests | `build/host/` |
| `make test TARGET=host` | Run comprehensive test suite | Tests execute automatically |
| `make help` | Show all available targets | - |

### Key File Locations
- **Headers:** ALL in `include/` hierarchy (never in `src/`)
- **Platform types:** `include/platform/platform.h` (mandatory first include)
- **Build outputs:** `build/amiga/` vs `build/host/` (never mix)

---

## Critical Integration Patterns

### Platform Abstraction Rules  
- **File I/O:** Use platform wrappers - never direct stdio
- **Memory:** Use platform memory allocation wrappers
- **Directory ops:** Use platform directory scanning functions
- **CLI isolation:** Amiga-specific code uses `#if PLATFORM_AMIGA`

---

## Language & Compiler Flags

| Target | Compiler | Key switches |
|--------|----------|-------------|
| **Amiga** | `vc` (vbcc driver) | `+aos68k -cpu=68000 -std=c99 -pedantic -Wall` |
| **Host**  | gcc         | `-std=c99 -pedantic -Wall -Wextra` |

*Both targets must build warning-free; treat warnings as errors.*

---

## Platform Header (mandatory include)

Every source file must include:

```c
#include <platform.h>
```

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