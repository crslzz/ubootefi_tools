/*
 * ubootefi_edit.c - Add or remove EFI variables in U-Boot EFI variable files
 *
 * Compile: gcc -o ubootefi_edit ubootefi_edit.c
 * Usage:
 *   Add:    ./ubootefi_edit <file> add <name> <guid> <attr> <data|@file> [time]
 *   Remove: ./ubootefi_edit <file> remove <name> <guid>
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>

#define EFI_VAR_FILE_MAGIC 0x0161566966456255ULL

/* EFI Variable Attributes */
#define EFI_VARIABLE_NON_VOLATILE                          0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                    0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS                        0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                 0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS            0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x00000020
#define EFI_VARIABLE_APPEND_WRITE                          0x00000040
#define EFI_VARIABLE_READ_ONLY                             0x80000000

/* GUID structure */
typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];
} __attribute__((packed)) efi_guid_t;

/* File header */
struct efi_var_file {
    uint64_t reserved;
    uint64_t magic;
    uint32_t length;
    uint32_t crc32;
} __attribute__((packed));

/* CRC32 table for checksum calculation */
static uint32_t crc32_table[256];

/* Initialize CRC32 lookup table */
void init_crc32_table(void)
{
    uint32_t poly = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? poly : 0);
        }
        crc32_table[i] = crc;
    }
}

/* Calculate CRC32 checksum */
uint32_t calculate_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/* Parse GUID from string format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX */
int parse_guid(const char *str, efi_guid_t *guid)
{
    unsigned int d1, d2, d3, d4[8];

    if (sscanf(str, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               &d1, &d2, &d3,
               &d4[0], &d4[1], &d4[2], &d4[3],
               &d4[4], &d4[5], &d4[6], &d4[7]) != 11) {
        return -1;
    }

    guid->data1 = d1;
    guid->data2 = d2;
    guid->data3 = d3;
    for (int i = 0; i < 8; i++) {
        guid->data4[i] = d4[i];
    }

    return 0;
}

/* Parse attribute flags from hex or symbolic names */
uint32_t parse_attributes(const char *str)
{
    /* Try parsing as hex number first */
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        return (uint32_t)strtoul(str, NULL, 16);
    }

    /* Parse symbolic names: NV,BS,RT,etc */
    uint32_t attr = 0;
    char *str_copy = strdup(str);
    char *token = strtok(str_copy, ",|");

    while (token) {
        /* Trim whitespace */
        while (*token && isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';

        if (strcasecmp(token, "NV") == 0)
            attr |= EFI_VARIABLE_NON_VOLATILE;
        else if (strcasecmp(token, "BS") == 0)
            attr |= EFI_VARIABLE_BOOTSERVICE_ACCESS;
        else if (strcasecmp(token, "RT") == 0)
            attr |= EFI_VARIABLE_RUNTIME_ACCESS;
        else if (strcasecmp(token, "HW_ERR") == 0)
            attr |= EFI_VARIABLE_HARDWARE_ERROR_RECORD;
        else if (strcasecmp(token, "AW") == 0)
            attr |= EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS;
        else if (strcasecmp(token, "AT") == 0)
            attr |= EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
        else if (strcasecmp(token, "APPEND") == 0)
            attr |= EFI_VARIABLE_APPEND_WRITE;
        else if (strcasecmp(token, "RO") == 0)
            attr |= EFI_VARIABLE_READ_ONLY;
        else {
            fprintf(stderr, "Warning: Unknown attribute '%s'\n", token);
        }

        token = strtok(NULL, ",|");
    }

    free(str_copy);
    return attr;
}

/* Convert ASCII string to UTF-16LE */
uint16_t *ascii_to_utf16le(const char *ascii, size_t *out_len)
{
    size_t len = strlen(ascii);
    uint16_t *utf16 = calloc(len + 1, sizeof(uint16_t));
    if (!utf16) return NULL;

    for (size_t i = 0; i < len; i++) {
        utf16[i] = (uint16_t)ascii[i];
    }
    utf16[len] = 0;

    *out_len = len;
    return utf16;
}

/* Convert UTF-16LE to ASCII */
char *utf16le_to_ascii(const uint16_t *utf16)
{
    size_t len = 0;
    while (utf16[len] != 0) len++;

    char *ascii = malloc(len + 1);
    if (!ascii) return NULL;

    for (size_t i = 0; i < len; i++) {
        ascii[i] = (char)(utf16[i] & 0xFF);
    }
    ascii[len] = '\0';

    return ascii;
}

/* Parse hex data from string (format: "aabbccdd" or "aa bb cc dd") */
uint8_t *parse_hex_data(const char *str, size_t *out_len)
{
    size_t str_len = strlen(str);
    uint8_t *data = malloc(str_len / 2 + 1);
    if (!data) return NULL;

    size_t data_len = 0;
    const char *p = str;

    while (*p) {
        while (*p && isspace(*p)) p++;
        if (!*p) break;

        if (!isxdigit(p[0]) || !isxdigit(p[1])) {
            fprintf(stderr, "Error: Invalid hex data at position %ld\n", p - str);
            free(data);
            return NULL;
        }

        char hex[3] = {p[0], p[1], 0};
        data[data_len++] = (uint8_t)strtoul(hex, NULL, 16);
        p += 2;
    }

    *out_len = data_len;
    return data;
}

/* Read data from file */
uint8_t *read_file_data(const char *filename, size_t *out_len)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", filename, strerror(errno));
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(fp);
        return NULL;
    }

    if (fread(data, 1, file_size, fp) != file_size) {
        fprintf(stderr, "Error: Failed to read file '%s'\n", filename);
        free(data);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    *out_len = file_size;
    return data;
}

/* Compare two GUIDs */
int guid_equal(const efi_guid_t *a, const efi_guid_t *b)
{
    return memcmp(a, b, sizeof(efi_guid_t)) == 0;
}

/* Find variable in file data, returns offset or -1 if not found */
ssize_t find_variable(const uint8_t *file_data, size_t file_size,
                      const char *name, const efi_guid_t *guid)
{
    size_t offset = sizeof(struct efi_var_file);

    while (offset + 32 < file_size) {
        uint32_t length, attr;
        efi_guid_t entry_guid;

        memcpy(&length, file_data + offset, 4);
        memcpy(&attr, file_data + offset + 4, 4);
        memcpy(&entry_guid, file_data + offset + 16, 16);

        if (length > file_size - offset - 32 || attr == 0 || attr > 0x80000040) {
            offset++;
            continue;
        }

        const uint16_t *entry_name = (const uint16_t *)(file_data + offset + 32);
        size_t name_len = 0;
        while (name_len < 200 && offset + 32 + (name_len + 1) * 2 < file_size
               && entry_name[name_len] != 0) {
            name_len++;
        }

        if (name_len == 0) {
            offset++;
            continue;
        }

        /* Check if this is the variable we're looking for */
        char *entry_name_ascii = utf16le_to_ascii(entry_name);
        int match = (strcmp(entry_name_ascii, name) == 0) && guid_equal(&entry_guid, guid);
        free(entry_name_ascii);

        if (match) {
            return offset;
        }

        /* Calculate entry size and move to next */
        size_t name_bytes = (name_len + 1) * 2;
        size_t entry_size = 32 + name_bytes + length;
        if (entry_size % 8 != 0) {
            entry_size += 8 - (entry_size % 8);
        }
        offset += entry_size;
    }

    return -1;
}

/* Add variable to file */
int add_variable(const char *filename, const char *name, const efi_guid_t *guid,
                 uint32_t attr, const uint8_t *data, size_t data_len, uint64_t time)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", filename, strerror(errno));
        return 1;
    }

    /* Read existing file */
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(fp);
        return 1;
    }

    if (fread(file_data, 1, file_size, fp) != file_size) {
        fprintf(stderr, "Error: Failed to read file\n");
        free(file_data);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    /* Verify file header */
    struct efi_var_file header;
    memcpy(&header, file_data, sizeof(header));

    if (header.magic != EFI_VAR_FILE_MAGIC) {
        fprintf(stderr, "Error: Invalid magic number\n");
        free(file_data);
        return 1;
    }

    /* Check if variable already exists */
    ssize_t existing_offset = find_variable(file_data, file_size, name, guid);
    if (existing_offset >= 0) {
        fprintf(stderr, "Error: Variable '%s' already exists. Remove it first.\n", name);
        free(file_data);
        return 1;
    }

    /* Create new variable entry */
    size_t name_len;
    uint16_t *name_utf16 = ascii_to_utf16le(name, &name_len);
    if (!name_utf16) {
        free(file_data);
        return 1;
    }

    size_t name_bytes = (name_len + 1) * 2;
    size_t entry_size_unpadded = 32 + name_bytes + data_len;
    size_t entry_size = entry_size_unpadded;
    if (entry_size % 8 != 0) {
        entry_size += 8 - (entry_size % 8);
    }

    /* Allocate new file data */
    size_t new_file_size = file_size + entry_size;
    uint8_t *new_file_data = calloc(1, new_file_size);
    if (!new_file_data) {
        free(name_utf16);
        free(file_data);
        return 1;
    }

    /* Copy old data */
    memcpy(new_file_data, file_data, file_size);

    /* Write new entry at end */
    size_t offset = file_size;
    memcpy(new_file_data + offset, &data_len, 4);
    memcpy(new_file_data + offset + 4, &attr, 4);
    memcpy(new_file_data + offset + 8, &time, 8);
    memcpy(new_file_data + offset + 16, guid, 16);
    memcpy(new_file_data + offset + 32, name_utf16, name_bytes);
    memcpy(new_file_data + offset + 32 + name_bytes, data, data_len);

    /* Update file header */
    uint32_t new_length = new_file_size;
    memcpy(new_file_data + 16, &new_length, 4);

    /* Calculate new CRC32 (over all entries, excluding 24-byte header) */
    init_crc32_table();
    uint32_t new_crc = calculate_crc32(new_file_data + 24, new_file_size - 24);
    memcpy(new_file_data + 20, &new_crc, 4);

    /* Write new file */
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file for writing: %s\n", strerror(errno));
        free(name_utf16);
        free(file_data);
        free(new_file_data);
        return 1;
    }

    if (fwrite(new_file_data, 1, new_file_size, fp) != new_file_size) {
        fprintf(stderr, "Error: Failed to write file\n");
        fclose(fp);
        free(name_utf16);
        free(file_data);
        free(new_file_data);
        return 1;
    }

    fclose(fp);
    free(name_utf16);
    free(file_data);
    free(new_file_data);

    printf("Successfully added variable '%s'\n", name);
    printf("  Data size: %zu bytes\n", data_len);
    printf("  New file size: %zu bytes\n", new_file_size);

    return 0;
}

/* Remove variable from file */
int remove_variable(const char *filename, const char *name, const efi_guid_t *guid)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", filename, strerror(errno));
        return 1;
    }

    /* Read existing file */
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(fp);
        return 1;
    }

    if (fread(file_data, 1, file_size, fp) != file_size) {
        fprintf(stderr, "Error: Failed to read file\n");
        free(file_data);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    /* Verify file header */
    struct efi_var_file header;
    memcpy(&header, file_data, sizeof(header));

    if (header.magic != EFI_VAR_FILE_MAGIC) {
        fprintf(stderr, "Error: Invalid magic number\n");
        free(file_data);
        return 1;
    }

    /* Find variable */
    ssize_t var_offset = find_variable(file_data, file_size, name, guid);
    if (var_offset < 0) {
        fprintf(stderr, "Error: Variable '%s' not found\n", name);
        free(file_data);
        return 1;
    }

    /* Calculate entry size */
    uint32_t length;
    memcpy(&length, file_data + var_offset, 4);

    const uint16_t *entry_name = (const uint16_t *)(file_data + var_offset + 32);
    size_t name_len = 0;
    while (entry_name[name_len] != 0) name_len++;

    size_t name_bytes = (name_len + 1) * 2;
    size_t entry_size = 32 + name_bytes + length;
    if (entry_size % 8 != 0) {
        entry_size += 8 - (entry_size % 8);
    }

    /* Create new file data without this entry */
    size_t new_file_size = file_size - entry_size;
    uint8_t *new_file_data = malloc(new_file_size);
    if (!new_file_data) {
        free(file_data);
        return 1;
    }

    /* Copy header */
    memcpy(new_file_data, file_data, 24);

    /* Copy data before variable */
    size_t before_size = var_offset - 24;
    if (before_size > 0) {
        memcpy(new_file_data + 24, file_data + 24, before_size);
    }

    /* Copy data after variable */
    size_t after_offset = var_offset + entry_size;
    size_t after_size = file_size - after_offset;
    if (after_size > 0) {
        memcpy(new_file_data + var_offset, file_data + after_offset, after_size);
    }

    /* Update file header */
    uint32_t new_length = new_file_size;
    memcpy(new_file_data + 16, &new_length, 4);

    /* Calculate new CRC32 */
    init_crc32_table();
    uint32_t new_crc = calculate_crc32(new_file_data + 24, new_file_size - 24);
    memcpy(new_file_data + 20, &new_crc, 4);

    /* Write new file */
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file for writing: %s\n", strerror(errno));
        free(file_data);
        free(new_file_data);
        return 1;
    }

    if (fwrite(new_file_data, 1, new_file_size, fp) != new_file_size) {
        fprintf(stderr, "Error: Failed to write file\n");
        fclose(fp);
        free(file_data);
        free(new_file_data);
        return 1;
    }

    fclose(fp);
    free(file_data);
    free(new_file_data);

    printf("Successfully removed variable '%s'\n", name);
    printf("  Entry size: %zu bytes\n", entry_size);
    printf("  New file size: %zu bytes\n", new_file_size);

    return 0;
}

void print_usage(const char *prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Add variable:\n");
    fprintf(stderr, "    %s <file> add <name> <guid> <attr> <data|@file> [time]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "  Remove variable:\n");
    fprintf(stderr, "    %s <file> remove <name> <guid>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  <file>      Path to ubootefi.var file\n");
    fprintf(stderr, "  <name>      Variable name (ASCII)\n");
    fprintf(stderr, "  <guid>      GUID in format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\n");
    fprintf(stderr, "  <attr>      Attributes as hex (0x00000007) or symbolic (NV,BS,RT)\n");
    fprintf(stderr, "  <data>      Hex data (e.g., 'aabbccdd' or 'aa bb cc dd')\n");
    fprintf(stderr, "  @<file>     Read data from file (prefix with @)\n");
    fprintf(stderr, "  [time]      Optional: timestamp (default: 0)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Common GUIDs:\n");
    fprintf(stderr, "  EFI_GLOBAL_VARIABLE:         8be4df61-93ca-11d2-aa0d-00e098032b8c\n");
    fprintf(stderr, "  EFI_IMAGE_SECURITY_DATABASE: d719b2cb-3d3a-4596-a3bc-dad00e67656f\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Common Attributes:\n");
    fprintf(stderr, "  NV  = Non-Volatile (0x00000001)\n");
    fprintf(stderr, "  BS  = Boot Service Access (0x00000002)\n");
    fprintf(stderr, "  RT  = Runtime Access (0x00000004)\n");
    fprintf(stderr, "  AT  = Time-based Authenticated Write (0x00000020)\n");
    fprintf(stderr, "  Standard: NV,BS,RT or 0x00000007\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  # Add variable with hex data\n");
    fprintf(stderr, "  %s ubootefi.var add TestVar \\\n", prog);
    fprintf(stderr, "    8be4df61-93ca-11d2-aa0d-00e098032b8c NV,BS,RT '48656c6c6f'\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  # Add variable from file\n");
    fprintf(stderr, "  %s ubootefi.var add MyVar \\\n", prog);
    fprintf(stderr, "    8be4df61-93ca-11d2-aa0d-00e098032b8c 0x00000007 @data.bin\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  # Remove variable\n");
    fprintf(stderr, "  %s ubootefi.var remove TestVar \\\n", prog);
    fprintf(stderr, "    8be4df61-93ca-11d2-aa0d-00e098032b8c\n");
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *operation = argv[2];

    if (strcmp(operation, "add") == 0) {
        if (argc < 7) {
            fprintf(stderr, "Error: Missing arguments for 'add' operation\n\n");
            print_usage(argv[0]);
            return 1;
        }

        const char *name = argv[3];
        const char *guid_str = argv[4];
        const char *attr_str = argv[5];
        const char *data_str = argv[6];
        const char *time_str = (argc > 7) ? argv[7] : "0";

        /* Parse GUID */
        efi_guid_t guid;
        if (parse_guid(guid_str, &guid) < 0) {
            fprintf(stderr, "Error: Invalid GUID format\n");
            return 1;
        }

        /* Parse attributes */
        uint32_t attr = parse_attributes(attr_str);
        if (attr == 0) {
            fprintf(stderr, "Error: Invalid or empty attributes\n");
            return 1;
        }

        /* Parse data */
        uint8_t *data;
        size_t data_len;

        if (data_str[0] == '@') {
            /* Read from file */
            data = read_file_data(data_str + 1, &data_len);
            if (!data) return 1;
        } else {
            /* Parse hex data */
            data = parse_hex_data(data_str, &data_len);
            if (!data) return 1;
        }

        /* Parse time */
        uint64_t time = strtoull(time_str, NULL, 0);

        /* Add variable */
        int ret = add_variable(filename, name, &guid, attr, data, data_len, time);
        free(data);
        return ret;

    } else if (strcmp(operation, "remove") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: Missing arguments for 'remove' operation\n\n");
            print_usage(argv[0]);
            return 1;
        }

        const char *name = argv[3];
        const char *guid_str = argv[4];

        /* Parse GUID */
        efi_guid_t guid;
        if (parse_guid(guid_str, &guid) < 0) {
            fprintf(stderr, "Error: Invalid GUID format\n");
            return 1;
        }

        /* Remove variable */
        return remove_variable(filename, name, &guid);

    } else {
        fprintf(stderr, "Error: Unknown operation '%s'\n\n", operation);
        print_usage(argv[0]);
        return 1;
    }
}
