# U-Boot EFI Variable Tools

Command-line utilities to parse, display, and edit U-Boot EFI variable files (`ubootefi.var`).

This repository contains two tools:
- **`ubootefi_dump`** - Parse and display EFI variables
- **`ubootefi_edit`** - Add or remove EFI variables

## Overview

### ubootefi_dump

A diagnostic tool for examining EFI variables stored by U-Boot's UEFI implementation. It parses the binary `ubootefi.var` file format and displays each variable's name, GUID, attributes, timestamp, and data in a human-readable format.

### ubootefi_edit

A management tool for modifying EFI variable files. It can add new variables or remove existing ones, automatically maintaining file integrity by updating CRC32 checksums and file headers.

These tools are useful for:
- Debugging U-Boot UEFI variable storage
- Inspecting secure boot configuration (PK, KEK, db, dbx)
- Examining boot options and boot order
- Modifying EFI variables without booting U-Boot
- Testing EFI variable handling
- Analyzing EFI variable corruption issues
- Understanding the ubootefi.var file format

## Features

### ubootefi_dump Features

- ✅ Parses U-Boot EFI variable files (v2026.04 format)
- ✅ Validates file magic number and CRC32 checksum
- ✅ Displays variable names, GUIDs, and attributes
- ✅ Decodes attribute flags (NV, BS, RT, AT, etc.)
- ✅ Shows hex dump of variable data
- ✅ Recognizes common EFI GUIDs (EFI_GLOBAL_VARIABLE, etc.)
- ✅ Handles UTF-16LE encoded variable names
- ✅ Supports authenticated variables with timestamps
- ✅ Robust parsing with error detection

### ubootefi_edit Features

- ✅ Add new EFI variables to existing files
- ✅ Remove existing EFI variables by name and GUID
- ✅ Accept variable data as hex strings or from files
- ✅ Support symbolic attribute names (NV,BS,RT) or hex values
- ✅ Automatically update CRC32 checksums
- ✅ Maintain 8-byte entry alignment
- ✅ Preserve file integrity
- ✅ Prevent duplicate variable names

## Installation

### Prerequisites

- GCC or Clang compiler
- GNU Make
- Linux, macOS, or other Unix-like system

### Build from Source

```bash
# Clone or download the source
cd ubootefi

# Build
make

# Test (if ubootefi.var file is present)
make test

# Install (optional)
sudo make install PREFIX=/usr/local
```

### Manual Compilation

```bash
gcc -o ubootefi_dump ubootefi_dump.c -Wall -Wextra -O2
```

## Usage

### ubootefi_dump - Basic Usage

```bash
./ubootefi_dump <ubootefi.var>
```

#### Example

```bash
$ ./ubootefi_dump ubootefi.var

=== U-Boot EFI Variable File ===
Magic:    0x0161566966456255 (valid)
Length:   1224 bytes
CRC32:    0x0746c792

=== EFI Variables ===

Variable #1:
  Name:       Boot0000
  GUID:       8be4df61-93ca-11d2-aa0d-00e098032b8c (EFI_GLOBAL_VARIABLE)
  Attributes: 0x00000007 [NV|BS|RT]
  Time:       0
  Data Size:  156 bytes (length field: 156)
  Entry Size: 208 bytes
  Offset:     0x18 (24)
  Data (hex): 01 00 00 00 74 00 76 00 69 00 72 00 74 00 69 00
              6f 00 20 00 30 00 00 00 01 04 1c 00 b9 73 1d e6 ... (124 more bytes)

Variable #2:
  Name:       BootOrder
  GUID:       8be4df61-93ca-11d2-aa0d-00e098032b8c (EFI_GLOBAL_VARIABLE)
  Attributes: 0x00000007 [NV|BS|RT]
  Time:       0
  Data Size:  2 bytes (length field: 2)
  Entry Size: 56 bytes
  Offset:     0xe8 (232)
  Data (hex): 00 00

...

Total variables found: 4
```

### ubootefi_edit - Basic Usage

```bash
# Add a variable with hex data
./ubootefi_edit <file> add <name> <guid> <attr> <hex-data> [time]

# Add a variable from a file
./ubootefi_edit <file> add <name> <guid> <attr> @<data-file> [time]

# Remove a variable
./ubootefi_edit <file> remove <name> <guid>
```

#### Add Variable Examples

```bash
# Add a simple variable with hex data (data is "Hello World!" in hex)
./ubootefi_edit ubootefi.var add TestVar \
  8be4df61-93ca-11d2-aa0d-00e098032b8c NV,BS,RT \
  '48656c6c6f20576f726c6421'

# Add a variable using hex attribute value instead of symbolic names
./ubootefi_edit ubootefi.var add TestVar2 \
  8be4df61-93ca-11d2-aa0d-00e098032b8c 0x00000007 \
  'aabbccdd'

# Add a variable from a binary file
./ubootefi_edit ubootefi.var add MyData \
  8be4df61-93ca-11d2-aa0d-00e098032b8c NV,BS,RT \
  @/path/to/data.bin

# Add an authenticated variable with a timestamp
./ubootefi_edit ubootefi.var add AuthVar \
  8be4df61-93ca-11d2-aa0d-00e098032b8c NV,BS,RT,AT \
  @cert.der 1774983695
```

#### Remove Variable Examples

```bash
# Remove a variable by name and GUID
./ubootefi_edit ubootefi.var remove TestVar \
  8be4df61-93ca-11d2-aa0d-00e098032b8c

# Remove a boot option
./ubootefi_edit ubootefi.var remove Boot0003 \
  8be4df61-93ca-11d2-aa0d-00e098032b8c
```

#### Common Attribute Values

| Symbolic | Hex Value | Description |
|----------|-----------|-------------|
| `NV,BS,RT` | `0x00000007` | Standard non-volatile variable (most common) |
| `NV,BS,RT,AT` | `0x00000027` | Authenticated variable with timestamp |
| `NV` | `0x00000001` | Non-volatile only |
| `BS,RT` | `0x00000006` | Boot service and runtime access |

#### Common GUIDs for Variables

| Name | GUID |
|------|------|
| EFI_GLOBAL_VARIABLE | `8be4df61-93ca-11d2-aa0d-00e098032b8c` |
| EFI_IMAGE_SECURITY_DATABASE | `d719b2cb-3d3a-4596-a3bc-dad00e67656f` |

### Locating the ubootefi.var File

The `ubootefi.var` file is typically stored on the EFI System Partition (ESP):

```bash
# Mount ESP (adjust device path as needed)
sudo mount /dev/sda1 /mnt/esp

# Locate the file
find /mnt/esp -name ubootefi.var

# Common locations:
# - /mnt/esp/ubootefi.var
# - /mnt/esp/EFI/ubootefi.var
# - /boot/efi/ubootefi.var

# Run the tool
./ubootefi_dump /mnt/esp/ubootefi.var
```

## Output Description

### File Header

- **Magic**: File format identifier (should be `0x0161566966456255`)
- **Length**: Total file size in bytes
- **CRC32**: Checksum of variable data (excludes header)

### Variable Entries

For each variable, the tool displays:

- **Name**: Variable name (e.g., Boot0000, BootOrder, PK)
- **GUID**: Vendor GUID identifying the variable namespace
- **Attributes**: Variable attribute flags:
  - `NV` - Non-volatile (persists across reboots)
  - `BS` - Bootservice access
  - `RT` - Runtime access
  - `AT` - Time-based authenticated write access
  - `AW` - Authenticated write access
  - `RO` - Read-only
- **Time**: Timestamp (for authenticated variables)
- **Data Size**: Size of variable data in bytes
- **Entry Size**: Total size of this entry including headers and padding
- **Offset**: Position in file
- **Data (hex)**: Hex dump of variable data (first 32 bytes)

### Common EFI Variables

| Variable Name | Description |
|---------------|-------------|
| `Boot0000`, `Boot0001`, ... | Boot options |
| `BootOrder` | Boot option order |
| `BootCurrent` | Currently booting option |
| `PlatformLang` | Platform language (e.g., "en-US") |
| `Timeout` | Boot menu timeout |
| `PK` | Platform Key (secure boot) |
| `KEK` | Key Exchange Keys (secure boot) |
| `db` | Authorized signature database |
| `dbx` | Forbidden signature database |

### Common GUIDs

| GUID | Name | Description |
|------|------|-------------|
| `8be4df61-93ca-11d2-aa0d-00e098032b8c` | EFI_GLOBAL_VARIABLE | Standard EFI variables |
| `d719b2cb-3d3a-4596-a3bc-dad00e67656f` | EFI_IMAGE_SECURITY_DATABASE | Secure boot databases |
| `a5c059a1-94e4-4aa7-87b5-ab155c2bf072` | EFI_CERT_X509 | X.509 certificate type |

## File Format

The `ubootefi.var` file format consists of:

1. **File Header** (24 bytes)
   - Reserved field (8 bytes)
   - Magic number (8 bytes): `0x0161566966456255`
   - Total length (4 bytes)
   - CRC32 checksum (4 bytes)

2. **Variable Entries** (variable length, sequential)
   - Length field (4 bytes) - **data size only**
   - Attributes (4 bytes)
   - Timestamp (8 bytes)
   - GUID (16 bytes)
   - Variable name (UTF-16LE, null-terminated)
   - Variable data (variable length)
   - Padding to 8-byte alignment

For detailed format specification and parsing issues, see [FORMAT.md](FORMAT.md).

### Important Note on Entry Length

⚠️ **The `length` field in each entry stores only the data size, not the total entry size.** The actual entry size must be calculated as:

```
entry_size = 32 + name_bytes + data_size + padding
```

where `padding` aligns the entry to an 8-byte boundary.

## Files in This Repository

- **`ubootefi_dump.c`** - Source code for the dump/parser tool
- **`ubootefi_edit.c`** - Source code for the variable editor tool
- **`Makefile`** - Build system for both tools
- **`FORMAT.md`** - Detailed file format specification and issues
- **`README.md`** - This file
- **`ubootefi.var`** - Example/test EFI variable file

## Makefile Targets

```bash
make              # Build both executables
make clean        # Remove build artifacts
make debug        # Build with debug symbols
make test         # Build and test dump tool on ubootefi.var
make install      # Install both tools to /usr/local/bin
make uninstall    # Uninstall both tools
make check        # Run static analysis (requires cppcheck)
make format       # Format code (requires clang-format)
make help         # Show help
make vars         # Display Makefile variables
```

## Development

### Building with Debug Symbols

```bash
make debug
gdb ./ubootefi_dump
```

### Custom Compiler

```bash
make CC=clang
```

### Custom Install Location

```bash
make install PREFIX=/usr
```

### Static Analysis

```bash
# Requires cppcheck
make check
```

## Known Issues and Limitations

1. **Misleading length field**: The entry `length` field stores data size only, not total entry size. This makes sequential parsing more complex than typical binary formats.

2. **No entry boundary markers**: The format lacks magic numbers between entries, making recovery from corruption difficult.

3. **UTF-16LE encoding**: Variable names are UTF-16LE encoded. The current implementation performs basic conversion to ASCII and may not handle all Unicode characters correctly.

4. **CRC32 validation**: The tool verifies the CRC32 but only warns on mismatch rather than failing, as the checksum excludes the file header.

See [FORMAT.md](FORMAT.md) for a complete list of format issues and inconsistencies.

## Compatibility

- **Tested with**: U-Boot v2026.04 format
- **Platform**: Linux (should work on any Unix-like system)
- **Architecture**: Architecture-independent (handles endianness correctly)

## Use Cases

### Debugging Boot Issues

```bash
# Check boot order
./ubootefi_dump /boot/efi/ubootefi.var | grep -A5 "BootOrder"

# List all boot options
./ubootefi_dump /boot/efi/ubootefi.var | grep "Boot[0-9]"
```

### Examining Secure Boot Configuration

```bash
# Check if Platform Key is set
./ubootefi_dump /boot/efi/ubootefi.var | grep -A10 "Name:.*PK"

# View secure boot variables
./ubootefi_dump /boot/efi/ubootefi.var | grep -E "PK|KEK|db|dbx"
```

### File Validation

```bash
# Verify file integrity
./ubootefi_dump /boot/efi/ubootefi.var | head -5
# Look for "Magic: ... (valid)" and verify CRC32
```

### Modifying Variables

```bash
# Backup the original file first!
cp /boot/efi/ubootefi.var /boot/efi/ubootefi.var.backup

# Add a custom boot option
./ubootefi_edit /boot/efi/ubootefi.var add Boot0010 \
  8be4df61-93ca-11d2-aa0d-00e098032b8c NV,BS,RT \
  @boot_option.bin

# Remove an old boot option
./ubootefi_edit /boot/efi/ubootefi.var remove Boot0009 \
  8be4df61-93ca-11d2-aa0d-00e098032b8c

# Verify the changes
./ubootefi_dump /boot/efi/ubootefi.var | grep Boot0010
```

### Testing and Development

```bash
# Create a minimal test file with one variable
cp ubootefi.var test.var
./ubootefi_edit test.var add TestData \
  8be4df61-93ca-11d2-aa0d-00e098032b8c NV,BS,RT \
  '0102030405060708'

# Verify it was added correctly
./ubootefi_dump test.var
```

## Troubleshooting

### ubootefi_dump Issues

#### "Error: Invalid magic number"

The file is not a valid ubootefi.var file or is corrupted.

#### "Warning: CRC32 mismatch"

The file may be corrupted or modified. The tool will still attempt to parse it.

#### "Warning: File size mismatch"

The file header's length field doesn't match the actual file size. This may indicate truncation or corruption.

#### No variables found

- Check that the file is not empty
- Verify the file is from U-Boot (not another UEFI implementation)
- The file may be corrupted

### ubootefi_edit Issues

#### "Error: Variable already exists"

You're trying to add a variable that already exists. Remove it first with the `remove` command, or use a different name.

#### "Error: Variable not found"

The variable you're trying to remove doesn't exist, or you provided the wrong GUID.

#### "Error: Invalid GUID format"

The GUID must be in format: `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX` (8-4-4-4-12 hex digits).

#### "Error: Invalid hex data"

Hex data must contain only hex digits (0-9, a-f, A-F), optionally with spaces. Example: `aabbccdd` or `aa bb cc dd`.

### Safety Tips

⚠️ **Always backup** the original `ubootefi.var` file before modifying it:

```bash
cp ubootefi.var ubootefi.var.backup
```

⚠️ **Test changes** on a copy first before modifying the actual system file:

```bash
cp /boot/efi/ubootefi.var ~/test.var
./ubootefi_edit ~/test.var add TestVar ...
./ubootefi_dump ~/test.var
# If it looks good, apply to the real file
```

## Contributing

Contributions are welcome! Please:

1. Follow the existing code style
2. Add tests for new features
3. Update documentation as needed
4. Run `make check` before submitting

## References

- [U-Boot Documentation](https://docs.u-boot.org/)
- [U-Boot Source: include/efi_variable.h](https://github.com/u-boot/u-boot/blob/master/include/efi_variable.h)
- [U-Boot Source: lib/efi_loader/efi_var_file.c](https://github.com/u-boot/u-boot/blob/master/lib/efi_loader/efi_var_file.c)
- [UEFI Specification](https://uefi.org/specifications)
- [EFI Variable Services](https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html#variable-services)

## License

This project is provided as-is for educational and debugging purposes.

## Author

Created by Claude Code powered by Claude Sonnet 4.5 (Anthropic AI) as a diagnostic tool for analyzing U-Boot EFI variable storage.

This code was generated in collaboration with a human user to parse and document the U-Boot EFI variable file format.

## Version

**Version**: 1.0
**Date**: 2026-04-21
**Compatible with**: U-Boot v2026.04 EFI variable format
