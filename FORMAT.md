# U-Boot EFI Variable File Format

## Overview

The `ubootefi.var` file is a binary format used by U-Boot's UEFI implementation to store non-volatile EFI variables. The file consists of a header followed by a sequential list of variable entries.

**File Format Version**: 1 (identified by magic number)
**Byte Order**: Little-endian
**Alignment**: Variable entries are padded to 8-byte boundaries

---

## File Structure

```
+-------------------+
| File Header       |  24 bytes
| (efi_var_file)    |
+-------------------+
| Variable Entry #1 |  Variable length
+-------------------+
| Variable Entry #2 |  Variable length
+-------------------+
|       ...         |
+-------------------+
| Variable Entry #N |  Variable length
+-------------------+
```

---

## File Header (24 bytes)

### Structure: `struct efi_var_file`

| Offset | Size | Field      | Type   | Description                                    |
|--------|------|------------|--------|------------------------------------------------|
| 0x00   | 8    | reserved   | uint64 | Reserved field, must be 0x0000000000000000     |
| 0x08   | 8    | magic      | uint64 | Magic number: 0x0161566966456255 ("UbEfiVa\x01") |
| 0x10   | 4    | length     | uint32 | Total file size in bytes (including header)    |
| 0x14   | 4    | crc32      | uint32 | CRC32 checksum of all variable entries (excluding header) |

### Magic Number

The magic number `0x0161566966456255` represents the ASCII string "UbEfiVa" followed by version byte 0x01:
- Bytes: `55 62 45 66 69 56 61 01`
- String: "UbEfiVa\x01"
- Constant: `EFI_VAR_FILE_MAGIC`

### CRC32 Calculation

- Algorithm: Standard CRC32 (polynomial 0xEDB88320)
- Initial value: 0xFFFFFFFF
- Data: All bytes after the header (offset 0x18 to end of file)
- Final XOR: 0xFFFFFFFF

---

## Variable Entry Structure

### Entry Layout

Each variable entry consists of:
1. **Entry Header** (32 bytes fixed)
2. **Variable Name** (variable length, UTF-16LE, null-terminated)
3. **Variable Data** (variable length)
4. **Padding** (0-7 bytes to align to 8-byte boundary)

### Entry Header (32 bytes)

| Offset | Size | Field      | Type      | Description                                    |
|--------|------|------------|-----------|------------------------------------------------|
| +0     | 4    | length     | uint32    | **Data length only** (NOT total entry size)    |
| +4     | 4    | attr       | uint32    | EFI variable attributes (bitfield)             |
| +8     | 8    | time       | uint64    | Timestamp (seconds since epoch, for authenticated vars) |
| +16    | 16   | guid       | efi_guid_t| Vendor GUID (128-bit UUID)                     |
| +32    | var  | name       | uint16[]  | Variable name in UTF-16LE, null-terminated     |
| +var   | var  | data       | uint8[]   | Variable data (size specified by length field) |
| +var   | 0-7  | padding    | uint8[]   | Null padding to 8-byte alignment               |

### GUID Structure (16 bytes)

Standard EFI GUID format:

| Offset | Size | Field  | Type    |
|--------|------|--------|---------|
| +0     | 4    | data1  | uint32  |
| +4     | 2    | data2  | uint16  |
| +6     | 2    | data3  | uint16  |
| +8     | 8    | data4  | uint8[8]|

**Format**: `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`

**Common GUIDs**:
- EFI Global Variable: `8be4df61-93ca-11d2-aa0d-00e098032b8c`
- EFI Image Security Database: `d719b2cb-3d3a-4596-a3bc-dad00e67656f`
- EFI X.509 Certificate: `a5c059a1-94e4-4aa7-87b5-ab155c2bf072`

### Attribute Flags (32-bit bitfield)

| Bit(s)  | Value      | Name                                      | Description                          |
|---------|------------|-------------------------------------------|--------------------------------------|
| 0       | 0x00000001 | EFI_VARIABLE_NON_VOLATILE                | Variable persists across reboots     |
| 1       | 0x00000002 | EFI_VARIABLE_BOOTSERVICE_ACCESS          | Accessible during boot services      |
| 2       | 0x00000004 | EFI_VARIABLE_RUNTIME_ACCESS              | Accessible at runtime                |
| 3       | 0x00000008 | EFI_VARIABLE_HARDWARE_ERROR_RECORD       | Hardware error record                |
| 4       | 0x00000010 | EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS  | Requires authentication to write     |
| 5       | 0x00000020 | EFI_VARIABLE_TIME_BASED_AUTHENTICATED... | Time-based authenticated write       |
| 6       | 0x00000040 | EFI_VARIABLE_APPEND_WRITE                | Append write supported               |
| 31      | 0x80000000 | EFI_VARIABLE_READ_ONLY                   | Read-only variable                   |

**Common Combinations**:
- `0x00000007` = NV | BS | RT (standard non-volatile variable)
- `0x00000027` = NV | BS | RT | AT (authenticated variable with timestamp)

### Entry Size Calculation

⚠️ **CRITICAL**: The `length` field contains the **data size only**, not the total entry size.

**Total entry size formula**:
```
entry_size = 32 + name_bytes + data_size + padding

where:
  name_bytes = (name_length_in_chars + 1) * 2  // UTF-16LE with null terminator
  data_size = length field value
  padding = (8 - (32 + name_bytes + data_size) % 8) % 8
```

---

## Parsing Example

```c
// Parse variable entries
size_t offset = sizeof(struct efi_var_file);  // Skip header

while (offset < file_size) {
    // Read entry header (32 bytes)
    uint32_t data_length, attr;
    uint64_t time;
    efi_guid_t guid;

    memcpy(&data_length, data + offset, 4);
    memcpy(&attr, data + offset + 4, 4);
    memcpy(&time, data + offset + 8, 8);
    memcpy(&guid, data + offset + 16, 16);

    // Read variable name (UTF-16LE)
    const uint16_t *name = (uint16_t *)(data + offset + 32);
    size_t name_len = utf16_strlen(name);
    size_t name_bytes = (name_len + 1) * 2;

    // Calculate entry size
    size_t entry_size = 32 + name_bytes + data_length;
    size_t padding = (8 - (entry_size % 8)) % 8;
    entry_size += padding;

    // Read variable data
    const uint8_t *var_data = data + offset + 32 + name_bytes;

    // Process variable...

    // Move to next entry
    offset += entry_size;
}
```

---

## References

- [U-Boot include/efi_variable.h](https://github.com/u-boot/u-boot/blob/master/include/efi_variable.h)
- [U-Boot lib/efi_loader/efi_var_file.c](https://elixir.bootlin.com/u-boot/latest/source/lib/efi_loader/efi_var_file.c)
- UEFI Specification 2.10
- EFI Variable Services

---

**Document Version**: 1.0
**Date**: 2026-04-21
**Analysis Tool**: ubootefi_dump.c
