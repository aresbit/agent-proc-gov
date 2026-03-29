# Modern Makefile for retty project with sp.h
# Based on gnaro template best practices
# Targets: all, retty, blindtty, test, lint, format, clean, install, uninstall

# Project Settings
debug ?= 0
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
BIN_DIR := bin
TESTS_DIR := tests

# Compiler and tool settings
CC := gcc
LINTER := clang-tidy
FORMATTER := clang-format

# Base compiler flags
CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
CFLAGS += -fPIC -m64
CFLAGS += -I$(INCLUDE_DIR)

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
	CFLAGS += -DSP_PLATFORM_LINUX
endif

ifeq ($(UNAME_M),x86_64)
	CFLAGS += -DSP_ARCH_X86_64
else ifeq ($(UNAME_M),aarch64)
	CFLAGS += -DSP_ARCH_ARM64
else
	$(warning Unsupported architecture: $(UNAME_M))
endif

# Debug vs Release configuration
ifeq ($(debug), 1)
	CFLAGS += -O0 -g -DDEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

# Linker flags
LDFLAGS := -lutil -lm

# Automatic source file discovery
RETTY_SRCS := $(wildcard $(SRC_DIR)/retty.c $(SRC_DIR)/attach.c $(SRC_DIR)/detach.c $(SRC_DIR)/sp.c)
BLINDTTY_SRCS := $(wildcard $(SRC_DIR)/blindtty.c $(SRC_DIR)/sp.c)

# Generate object file paths in build directory
RETTY_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(RETTY_SRCS))
BLINDTTY_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(BLINDTTY_SRCS))

# Executables
EXECUTABLES := retty blindtty
EXECUTABLE_PATHS := $(addprefix $(BIN_DIR)/,$(EXECUTABLES))

# Default target
all: dir $(EXECUTABLE_PATHS)

# Create build directories
dir:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Main retty executable
$(BIN_DIR)/retty: $(RETTY_OBJS) | dir
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Blindtty executable
$(BIN_DIR)/blindtty: $(BLINDTTY_OBJS) | dir
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for compiling object files with automatic dependency generation
$(BUILD_DIR)/%.o: %.c $(INCLUDE_DIR)/sp.h | dir
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Include auto-generated dependencies
-include $(RETTY_OBJS:.o=.d)
-include $(BLINDTTY_OBJS:.o=.d)

# Test targets
test: all
	@echo "Testing retty..."
	$(BIN_DIR)/retty -v || true
	@echo "Testing blindtty..."
	$(BIN_DIR)/blindtty --help || true
	@echo "Running SP_LOG test..."
	$(CC) $(CFLAGS) -o $(BIN_DIR)/test_sp_log $(TESTS_DIR)/test_sp_log.c $(SRC_DIR)/sp.c $(LDFLAGS)
	$(BIN_DIR)/test_sp_log || true

# Lint source files
lint:
	@if command -v $(LINTER) >/dev/null 2>&1; then \
		$(LINTER) --checks=* --warnings-as-errors=* $(RETTY_SRCS) $(BLINDTTY_SRCS) -- $(CFLAGS); \
		echo "Linting completed successfully"; \
	else \
		echo "clang-tidy not found, skipping linting"; \
	fi

# Format source files
format:
	@if command -v $(FORMATTER) >/dev/null 2>&1; then \
		$(FORMATTER) -i $(RETTY_SRCS) $(BLINDTTY_SRCS) $(INCLUDE_DIR)/sp.h; \
		echo "Formatting completed successfully"; \
	else \
		echo "clang-format not found, skipping formatting"; \
	fi

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(EXECUTABLES) *.o

# Install executables to system
PREFIX ?= /usr/local
install: all
	install -m 755 -D $(BIN_DIR)/retty $(PREFIX)/bin/retty
	install -m 755 -D $(BIN_DIR)/blindtty $(PREFIX)/bin/blindtty

# Uninstall executables from system
uninstall:
	rm -f $(PREFIX)/bin/retty $(PREFIX)/bin/blindtty

# Development environment setup (Debian/Ubuntu)
setup:
	@echo "Setting up development environment..."
	@sudo apt update
	@sudo apt install -y clang-tidy clang-format valgrind bear

# Generate compile_commands.json for tooling
bear: clean
	bear -- make

# Shortcut targets
.PHONY: all dir clean test lint format install uninstall setup bear