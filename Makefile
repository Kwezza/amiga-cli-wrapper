# Amiga CLI Wrapper Makefile
# Supports dual-target compilation: Amiga (vbcc) and Host (gcc)

# Default target
TARGET ?= amiga

# Project structure
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = test
BUILD_DIR = build

# Source files
CLI_WRAPPER_SOURCES = $(SRC_DIR)/cli_wrapper.c
TEST_SOURCES = $(TEST_DIR)/cli_wrapper_test.c

# Compiler settings per target
ifeq ($(TARGET),amiga)
    # Amiga target using vbcc
    CC = vc
    CFLAGS = +aos68k -cpu=68000 -I$(INCLUDE_DIR) -DPLATFORM_AMIGA
    LDFLAGS = -lamiga
    BUILD_TARGET_DIR = $(BUILD_DIR)/amiga
    EXECUTABLE_EXT =
    PLATFORM_DEFINE = -DPLATFORM_AMIGA
else
    # Host target using gcc
    CC = gcc
    CFLAGS = -std=c99 -pedantic -Wall -Wextra -I$(INCLUDE_DIR)
    LDFLAGS =
    BUILD_TARGET_DIR = $(BUILD_DIR)/host
    ifeq ($(OS),Windows_NT)
        EXECUTABLE_EXT = .exe
    else
        EXECUTABLE_EXT =
    endif
    PLATFORM_DEFINE =
endif

# Output files
CLI_WRAPPER_TEST = $(BUILD_TARGET_DIR)/cli_wrapper_test$(EXECUTABLE_EXT)

# Default target
.PHONY: all
all: build-test

# Create build directories
$(BUILD_TARGET_DIR):
	@echo "Creating build directory: $(BUILD_TARGET_DIR)"
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_TARGET_DIR))" mkdir "$(subst /,\,$(BUILD_TARGET_DIR))"
else
	@mkdir -p $(BUILD_TARGET_DIR)
endif

# Build the test executable
.PHONY: build-test
build-test: $(CLI_WRAPPER_TEST)

$(CLI_WRAPPER_TEST): $(CLI_WRAPPER_SOURCES) $(TEST_SOURCES) | $(BUILD_TARGET_DIR)
	@echo "Building CLI wrapper test for target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES) $(CLI_WRAPPER_SOURCES) $(LDFLAGS)
	@echo "Build completed: $@"

# Test target (host only)
.PHONY: test
test:
ifeq ($(TARGET),host)
	@echo "Running host tests..."
	$(MAKE) TARGET=host build-test
	@echo "Host test build completed (functionality stubbed)"
else
	@echo "Tests can only be run on host target"
	@echo "Use: make test TARGET=host"
endif

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
ifeq ($(OS),Windows_NT)
	@if exist "build" rmdir /s /q "build"
	@if exist "logfile.txt" del "logfile.txt"
else
	@rm -rf $(BUILD_DIR)
	@rm -f logfile.txt
endif
	@echo "Clean completed"

# Help target
.PHONY: help
help:
	@echo "Amiga CLI Wrapper Build System"
	@echo "=============================="
	@echo ""
	@echo "Targets:"
	@echo "  all              Build default target (amiga)"
	@echo "  build-test       Build CLI wrapper test program"
	@echo "  test             Run tests (host target only)"
	@echo "  clean            Remove all build artifacts"
	@echo "  help             Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  TARGET=amiga     Build for Amiga using vbcc (default)"
	@echo "  TARGET=host      Build for host using gcc"
	@echo ""
	@echo "Examples:"
	@echo "  make                          # Build for Amiga"
	@echo "  make TARGET=amiga             # Build for Amiga"
	@echo "  make TARGET=host              # Build for host"
	@echo "  make test TARGET=host         # Run host tests"
	@echo "  make clean                    # Clean all artifacts"
	@echo ""
	@echo "Output locations:"
	@echo "  Amiga binaries: $(BUILD_DIR)/amiga/"
	@echo "  Host binaries:  $(BUILD_DIR)/host/"

# Show current configuration
.PHONY: config
config:
	@echo "Current Configuration:"
	@echo "====================="
	@echo "Target:           $(TARGET)"
	@echo "Compiler:         $(CC)"
	@echo "C Flags:          $(CFLAGS)"
	@echo "Linker Flags:     $(LDFLAGS)"
	@echo "Build Directory:  $(BUILD_TARGET_DIR)"
	@echo "Executable Ext:   '$(EXECUTABLE_EXT)'"
	@echo "Platform Define:  $(PLATFORM_DEFINE)"
