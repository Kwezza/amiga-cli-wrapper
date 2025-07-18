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
CLI_WRAPPER_SOURCES = $(SRC_DIR)/cli_wrapper.c $(SRC_DIR)/process_control.c $(SRC_DIR)/lha_wrapper.c
TEST_SOURCES = $(TEST_DIR)/cli_wrapper_test.c
BYTES_TEST_SOURCES = $(TEST_DIR)/cli_bytes_test.c
PROCESS_CONTROL_TEST_SOURCES = $(TEST_DIR)/test_process_control.c
PAUSE_RESUME_TEST_SOURCES = $(TEST_DIR)/pause_resume_test.c
FILE_CORRUPTOR_SOURCES = $(SRC_DIR)/file_corruptor.c
FILE_CORRUPTOR_TEST_SOURCES = $(TEST_DIR)/file_corruptor_test.c

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
CLI_BYTES_TEST = $(BUILD_TARGET_DIR)/cli_bytes_test$(EXECUTABLE_EXT)
PROCESS_CONTROL_TEST = $(BUILD_TARGET_DIR)/test_process_control$(EXECUTABLE_EXT)
PAUSE_RESUME_TEST = $(BUILD_TARGET_DIR)/pause_resume_test$(EXECUTABLE_EXT)
FILE_CORRUPTOR = $(BUILD_TARGET_DIR)/file_corruptor$(EXECUTABLE_EXT)
FILE_CORRUPTOR_TEST = $(BUILD_TARGET_DIR)/file_corruptor_test$(EXECUTABLE_EXT)

# Default target
.PHONY: all
ifeq ($(TARGET),host)
all: build-test build-bytes-test build-process-control-test build-pause-resume-test build-file-corruptor build-file-corruptor-test
else
all: build-test build-bytes-test build-process-control-test build-pause-resume-test
endif

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
	@echo "Copying test assets..."
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_TARGET_DIR))\assets" mkdir "$(subst /,\,$(BUILD_TARGET_DIR))\assets"
	@copy "assets\A10TankKiller_v2.0_3Disk.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy test archive"
	@copy "assets\test_archive_corrupted.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy corrupted test archive"
else
	@mkdir -p $(BUILD_TARGET_DIR)/assets
	@cp assets/A10TankKiller_v2.0_3Disk.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy test archive"
	@cp assets/test_archive_corrupted.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy corrupted test archive"
endif

# Build the byte-level test executable
.PHONY: build-bytes-test
build-bytes-test: $(CLI_BYTES_TEST)

$(CLI_BYTES_TEST): $(CLI_WRAPPER_SOURCES) $(BYTES_TEST_SOURCES) | $(BUILD_TARGET_DIR)
	@echo "Building CLI bytes test for target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $@ $(BYTES_TEST_SOURCES) $(CLI_WRAPPER_SOURCES) $(LDFLAGS)
	@echo "Build completed: $@"
	@echo "Copying test assets..."
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_TARGET_DIR))\assets" mkdir "$(subst /,\,$(BUILD_TARGET_DIR))\assets"
	@copy "assets\A10TankKiller_v2.0_3Disk.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy test archive"
	@copy "assets\test_archive_corrupted.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy corrupted test archive"
else
	@mkdir -p $(BUILD_TARGET_DIR)/assets
	@cp assets/A10TankKiller_v2.0_3Disk.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy test archive"
	@cp assets/test_archive_corrupted.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy corrupted test archive"
endif

# Build the process control test executable
.PHONY: build-process-control-test
build-process-control-test: $(PROCESS_CONTROL_TEST)

$(PROCESS_CONTROL_TEST): $(CLI_WRAPPER_SOURCES) $(PROCESS_CONTROL_TEST_SOURCES) | $(BUILD_TARGET_DIR)
	@echo "Building process control test for target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $@ $(PROCESS_CONTROL_TEST_SOURCES) $(CLI_WRAPPER_SOURCES) $(LDFLAGS)
	@echo "Build completed: $@"
	@echo "Copying test assets..."
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_TARGET_DIR))\assets" mkdir "$(subst /,\,$(BUILD_TARGET_DIR))\assets"
	@copy "assets\A10TankKiller_v2.0_3Disk.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy test archive"
	@copy "assets\test_archive_corrupted.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy corrupted test archive"
else
	@mkdir -p $(BUILD_TARGET_DIR)/assets
	@cp assets/A10TankKiller_v2.0_3Disk.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy test archive"
	@cp assets/test_archive_corrupted.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy corrupted test archive"
endif

# Build the pause/resume test executable
.PHONY: build-pause-resume-test
build-pause-resume-test: $(PAUSE_RESUME_TEST)

$(PAUSE_RESUME_TEST): $(CLI_WRAPPER_SOURCES) $(PAUSE_RESUME_TEST_SOURCES) | $(BUILD_TARGET_DIR)
	@echo "Building pause/resume test for target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $@ $(PAUSE_RESUME_TEST_SOURCES) $(CLI_WRAPPER_SOURCES) $(LDFLAGS)
	@echo "Build completed: $@"
	@echo "Copying test assets..."
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_TARGET_DIR))\assets" mkdir "$(subst /,\,$(BUILD_TARGET_DIR))\assets"
	@copy "assets\A10TankKiller_v2.0_3Disk.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy test archive"
	@copy "assets\test_archive_corrupted.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy corrupted test archive"
else
	@mkdir -p $(BUILD_TARGET_DIR)/assets
	@cp assets/A10TankKiller_v2.0_3Disk.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy test archive"
	@cp assets/test_archive_corrupted.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy corrupted test archive"
endif

# Build the file corruptor utility (host only)
.PHONY: build-file-corruptor
build-file-corruptor: $(FILE_CORRUPTOR)

$(FILE_CORRUPTOR): $(FILE_CORRUPTOR_SOURCES) | $(BUILD_TARGET_DIR)
ifeq ($(TARGET),host)
	@echo "Building file corruptor utility for host target"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $@ $(FILE_CORRUPTOR_SOURCES) $(LDFLAGS)
	@echo "Build completed: $@"
else
	@echo "File corruptor is only available for host target"
	@echo "Use: make build-file-corruptor TARGET=host"
endif

# Build the file corruptor test (host only)
.PHONY: build-file-corruptor-test
build-file-corruptor-test: $(FILE_CORRUPTOR_TEST)

$(FILE_CORRUPTOR_TEST): $(FILE_CORRUPTOR_TEST_SOURCES) | $(BUILD_TARGET_DIR)
ifeq ($(TARGET),host)
	@echo "Building file corruptor test for host target"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $@ $(FILE_CORRUPTOR_TEST_SOURCES) $(LDFLAGS)
	@echo "Build completed: $@"
	@echo "Copying test assets..."
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_TARGET_DIR))\assets" mkdir "$(subst /,\,$(BUILD_TARGET_DIR))\assets"
	@copy "assets\A10TankKiller_v2.0_3Disk.lha" "$(subst /,\,$(BUILD_TARGET_DIR))\assets\" >nul 2>nul || echo "Warning: Could not copy test archive"
else
	@mkdir -p $(BUILD_TARGET_DIR)/assets
	@cp assets/A10TankKiller_v2.0_3Disk.lha $(BUILD_TARGET_DIR)/assets/ 2>/dev/null || echo "Warning: Could not copy test archive"
endif
else
	@echo "File corruptor test is only available for host target"
	@echo "Use: make build-file-corruptor-test TARGET=host"
endif

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
	@echo "  all                          Build default target (amiga)"
	@echo "  build-test                   Build CLI wrapper test program"
	@echo "  build-bytes-test             Build CLI byte-level test program"
	@echo "  build-process-control-test   Build process control test program"
	@echo "  build-pause-resume-test      Build pause/resume test program"
	@echo "  build-file-corruptor         Build file corruptor utility (host only)"
	@echo "  build-file-corruptor-test    Build file corruptor test program (host only)"
	@echo "  test                         Run tests (host target only)"
	@echo "  clean                        Remove all build artifacts"
	@echo "  help                         Show this help message"
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
