# ---------------- user-selectable platform -------------------
TARGET ?= amiga   # Default to Amiga target

# --- bundle stamping -------------------------------------------------
BUILD_NAME     ?= $(or $(BUILD_TAG),current)
BUILD_DIR      := build/$(TARGET)/$(BUILD_NAME)

# ---------------- compiler executables -----------------------
AMIGA_CC ?= vc
HOST_CC  ?= gcc             # or clang

# ---------------- directory helpers --------------------------
MKDIR_P = mkdir -p
COPY_R  = cp -R

# ---------------- common flags -------------------------------
AMIGA_CFLAGS := -DPLATFORM_AMIGA=1 +aos68k -O2 -c99 -I"C:/VBCC/targets/m68k-amigaos/include" -Isrc -Iinclude -Iinclude/platform
HOST_CFLAGS  := -DPLATFORM_HOST=1 -std=c99 -Wall -Wextra -Iinclude/platform -Isrc -Iinclude

# ---------------- automatic selection ------------------------
ifeq ($(TARGET),amiga)
  CC     := $(AMIGA_CC)
  CFLAGS := $(AMIGA_CFLAGS)
  LDFLAGS := -lamiga
  BIN_EXT :=
else
  CC     := $(HOST_CC)
  CFLAGS := $(HOST_CFLAGS)
  LDFLAGS :=
  BIN_EXT := .exe
endif

# ---------------- source files -------------------------------
CORE_SRCS = src/core/main.c \
            src/platform/platform_io.c

TOOL_SRCS = src/tools/example_tool.c

TEST_SRCS = tests/$(TARGET)/basic_test.c

# ---------------- object files -------------------------------
CORE_OBJS = $(CORE_SRCS:.c=.o)
TOOL_OBJS = $(TOOL_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

# ---------------- targets ------------------------------------
TARGET_NAME = amiga-cli-wrapper$(BIN_EXT)
TEST_NAME = test_runner$(BIN_EXT)

# ---------------- build rules --------------------------------

.PHONY: all clean help test

all: $(BUILD_DIR)/$(TARGET_NAME)

$(BUILD_DIR)/$(TARGET_NAME): $(CORE_OBJS) $(TOOL_OBJS) | build-dir
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(TOOL_OBJS)

test: $(BUILD_DIR)/$(TEST_NAME)
	@echo "Running $(TARGET) tests..."
ifeq ($(TARGET),host)
	$(BUILD_DIR)/$(TEST_NAME)
else
	@echo "Amiga tests built. Transfer to Amiga to run."
endif

$(BUILD_DIR)/$(TEST_NAME): $(TEST_OBJS) | build-dir
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS)

build-dir:
	$(MKDIR_P) $(BUILD_DIR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/
	rm -f $(CORE_OBJS) $(TOOL_OBJS) $(TEST_OBJS)

help:
	@echo "Amiga CLI Wrapper Build System"
	@echo "=============================="
	@echo ""
	@echo "MAIN TARGETS:"
	@echo "  all                       - Build main executable"
	@echo "  test                      - Build and run tests"
	@echo "  clean                     - Remove build artifacts"
	@echo "  help                      - Show this help"
	@echo ""
	@echo "BUILD CONFIGURATION:"
	@echo "  TARGET=amiga              - Build for Amiga (default)"
	@echo "  TARGET=host               - Build for host platform"
	@echo ""
	@echo "BUILD OUTPUT LOCATIONS:"
	@echo "  Amiga binaries: build/amiga/"
	@echo "  Host binaries:  build/host/"
	@echo ""
	@echo "REQUIREMENTS:"
	@echo "  Amiga: vbcc with +aos68k target"
	@echo "  Host:  gcc or compatible C compiler"

# End of Makefile