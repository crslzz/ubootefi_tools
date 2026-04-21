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
DUMP_SRC := ubootefi_dump.c
DUMP_OBJ := $(DUMP_SRC:.c=.o)
DUMP_TARGET := ubootefi_dump

EDIT_SRC := ubootefi_edit.c
EDIT_OBJ := $(EDIT_SRC:.c=.o)
EDIT_TARGET := ubootefi_edit

ALL_TARGETS := $(DUMP_TARGET) $(EDIT_TARGET)

# Default target
.PHONY: all
all: $(ALL_TARGETS)

# Build the executables
$(DUMP_TARGET): $(DUMP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Build complete: $(DUMP_TARGET)"

$(EDIT_TARGET): $(EDIT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Build complete: $(EDIT_TARGET)"

# Compile source to object
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Debug build
.PHONY: debug
debug: CFLAGS := $(DEBUGFLAGS) -Wall -Wextra -std=c11
debug: clean $(ALL_TARGETS)
	@echo "Debug build complete"

# Install the binaries
.PHONY: install
install: $(ALL_TARGETS)
	@echo "Installing to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(DUMP_TARGET) $(BINDIR)/$(DUMP_TARGET)
	install -m 755 $(EDIT_TARGET) $(BINDIR)/$(EDIT_TARGET)
	@echo "Installation complete"

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "Uninstalling from $(BINDIR)..."
	rm -f $(BINDIR)/$(DUMP_TARGET)
	rm -f $(BINDIR)/$(EDIT_TARGET)
	@echo "Uninstall complete"

# Clean build artifacts
.PHONY: clean
clean:
	rm -f $(DUMP_OBJ) $(DUMP_TARGET)
	rm -f $(EDIT_OBJ) $(EDIT_TARGET)
	@echo "Clean complete"

# Run the dump program on test file
.PHONY: test
test: $(DUMP_TARGET)
	@if [ -f ubootefi.var ]; then \
		echo "Running $(DUMP_TARGET) on ubootefi.var..."; \
		./$(DUMP_TARGET) ubootefi.var; \
	else \
		echo "Error: ubootefi.var not found"; \
		exit 1; \
	fi

# Check code with static analyzer
.PHONY: check
check: $(DUMP_SRC) $(EDIT_SRC)
	@echo "Running static analysis..."
	@which cppcheck > /dev/null 2>&1 && cppcheck --enable=all --suppress=missingIncludeSystem $(DUMP_SRC) $(EDIT_SRC) || echo "cppcheck not installed"

# Format code (requires clang-format)
.PHONY: format
format:
	@which clang-format > /dev/null 2>&1 && clang-format -i $(DUMP_SRC) $(EDIT_SRC) || echo "clang-format not installed"

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
	@echo "CC          = $(CC)"
	@echo "CFLAGS      = $(CFLAGS)"
	@echo "LDFLAGS     = $(LDFLAGS)"
	@echo "DUMP_SRC    = $(DUMP_SRC)"
	@echo "DUMP_OBJ    = $(DUMP_OBJ)"
	@echo "DUMP_TARGET = $(DUMP_TARGET)"
	@echo "EDIT_SRC    = $(EDIT_SRC)"
	@echo "EDIT_OBJ    = $(EDIT_OBJ)"
	@echo "EDIT_TARGET = $(EDIT_TARGET)"
	@echo "ALL_TARGETS = $(ALL_TARGETS)"
	@echo "PREFIX      = $(PREFIX)"
	@echo "BINDIR      = $(BINDIR)"
