# Makefile for ubootefi_dump
# U-Boot EFI variable file parser

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2 -std=c11
DEBUGFLAGS := -g -O0 -DDEBUG
LDFLAGS :=

# Directories
PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
MANDIR := $(PREFIX)/share/man/man1

# Source files
SRC := ubootefi_dump.c
OBJ := $(SRC:.c=.o)
TARGET := ubootefi_dump

# Default target
.PHONY: all
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Build complete: $(TARGET)"

# Compile source to object
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Debug build
.PHONY: debug
debug: CFLAGS := $(DEBUGFLAGS) -Wall -Wextra -std=c11
debug: clean $(TARGET)
	@echo "Debug build complete: $(TARGET)"

# Install the binary
.PHONY: install
install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "Installation complete"

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "Uninstalling $(TARGET) from $(BINDIR)..."
	rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstall complete"

# Clean build artifacts
.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)
	@echo "Clean complete"

# Run the program on test file
.PHONY: test
test: $(TARGET)
	@if [ -f ubootefi.var ]; then \
		echo "Running $(TARGET) on ubootefi.var..."; \
		./$(TARGET) ubootefi.var; \
	else \
		echo "Error: ubootefi.var not found"; \
		exit 1; \
	fi

# Check code with static analyzer
.PHONY: check
check: $(SRC)
	@echo "Running static analysis..."
	@which cppcheck > /dev/null 2>&1 && cppcheck --enable=all --suppress=missingIncludeSystem $(SRC) || echo "cppcheck not installed"

# Format code (requires clang-format)
.PHONY: format
format:
	@which clang-format > /dev/null 2>&1 && clang-format -i $(SRC) || echo "clang-format not installed"

# Show help
.PHONY: help
help:
	@echo "Makefile for ubootefi_dump"
	@echo ""
	@echo "Usage:"
	@echo "  make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all        Build the executable (default)"
	@echo "  debug      Build with debug symbols and no optimization"
	@echo "  clean      Remove build artifacts"
	@echo "  install    Install to $(BINDIR) (PREFIX=$(PREFIX))"
	@echo "  uninstall  Uninstall from $(BINDIR)"
	@echo "  test       Run the program on ubootefi.var"
	@echo "  check      Run static analysis (requires cppcheck)"
	@echo "  format     Format source code (requires clang-format)"
	@echo "  help       Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  CC         C compiler (default: gcc)"
	@echo "  CFLAGS     Compiler flags"
	@echo "  PREFIX     Install prefix (default: /usr/local)"
	@echo ""
	@echo "Examples:"
	@echo "  make                  # Build the program"
	@echo "  make debug            # Build with debug symbols"
	@echo "  make test             # Build and test"
	@echo "  make install PREFIX=/usr  # Install to /usr/bin"

# Print variables (for debugging the Makefile)
.PHONY: vars
vars:
	@echo "CC       = $(CC)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "SRC      = $(SRC)"
	@echo "OBJ      = $(OBJ)"
	@echo "TARGET   = $(TARGET)"
	@echo "PREFIX   = $(PREFIX)"
	@echo "BINDIR   = $(BINDIR)"
