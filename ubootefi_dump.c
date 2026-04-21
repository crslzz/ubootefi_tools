/*
 * ubootefi_dump_final.c - Simple and robust parser for U-Boot EFI variable files
 *
 * Compile: gcc -o ubootefi_dump ubootefi_dump_final.c
 * Usage: ./ubootefi_dump <ubootefi.var>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

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

/* Print GUID */
void print_guid(const efi_guid_t *guid)
{
    printf("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           guid->data1, guid->data2, guid->data3,
           guid->data4[0], guid->data4[1],
           guid->data4[2], guid->data4[3], guid->data4[4],
           guid->data4[5], guid->data4[6], guid->data4[7]);
}

/* Print GUID with name */
void print_guid_with_name(const efi_guid_t *guid)
{
    static const struct {
        efi_guid_t guid;
        const char *name;
    } known_guids[] = {
        {{0x8be4df61, 0x93ca, 0x11d2, {0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c}},
         "EFI_GLOBAL_VARIABLE"},
        {{0xd719b2cb, 0x3d3a, 0x4596, {0xa3, 0xbc, 0xda, 0xd0, 0x0e, 0x67, 0x65, 0x6f}},
         "EFI_IMAGE_SECURITY_DATABASE"},
        {{0xa5c059a1, 0x94e4, 0x4aa7, {0x87, 0xb5, 0xab, 0x15, 0x5c, 0x2b, 0xf0, 0x72}},
         "EFI_CERT_X509"},
    };

    print_guid(guid);
    for (size_t i = 0; i < sizeof(known_guids) / sizeof(known_guids[0]); i++) {
        if (memcmp(guid, &known_guids[i].guid, sizeof(efi_guid_t)) == 0) {
            printf(" (%s)", known_guids[i].name);
            return;
        }
    }
}

/* Print attributes */
void print_attributes(uint32_t attr)
{
    printf("0x%08x [", attr);
    int first = 1;
    if (attr & EFI_VARIABLE_NON_VOLATILE) {
        printf("%sNV", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_BOOTSERVICE_ACCESS) {
        printf("%sBS", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_RUNTIME_ACCESS) {
        printf("%sRT", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
        printf("%sHW_ERR", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) {
        printf("%sAW", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) {
        printf("%sAT", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_APPEND_WRITE) {
        printf("%sAPPEND", first ? "" : "|");
        first = 0;
    }
    if (attr & EFI_VARIABLE_READ_ONLY) {
        printf("%sRO", first ? "" : "|");
        first = 0;
    }
    printf("]");
}

/* Convert UTF-16LE to ASCII */
char *utf16le_to_ascii(const uint16_t *utf16, size_t max_words)
{
    static char buffer[512];
    size_t i;

    for (i = 0; i < max_words && i < (sizeof(buffer) - 1); i++) {
        if (utf16[i] == 0)
            break;
        buffer[i] = (char)(utf16[i] & 0xFF);
    }
    buffer[i] = '\0';
    return buffer;
}

int main(int argc, char *argv[])
{
    FILE *fp;
    struct efi_var_file header;
    uint8_t *file_data = NULL;
    size_t file_size;
    int ret = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ubootefi.var>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    file_data = malloc(file_size);
    if (!file_data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return 1;
    }

    if (fread(file_data, 1, file_size, fp) != file_size) {
        fprintf(stderr, "Error: Failed to read file\n");
        ret = 1;
        goto cleanup;
    }
    fclose(fp);

    if (file_size < sizeof(header)) {
        fprintf(stderr, "Error: File too small\n");
        ret = 1;
        goto cleanup;
    }

    memcpy(&header, file_data, sizeof(header));

    printf("=== U-Boot EFI Variable File ===\n");
    printf("Magic:    0x%016llx %s\n",
           (unsigned long long)header.magic,
           header.magic == EFI_VAR_FILE_MAGIC ? "(valid)" : "(INVALID!)");
    printf("Length:   %u bytes\n", header.length);
    printf("CRC32:    0x%08x\n\n", header.crc32);

    if (header.magic != EFI_VAR_FILE_MAGIC) {
        fprintf(stderr, "Error: Invalid magic number\n");
        ret = 1;
        goto cleanup;
    }

    /* Parse variables starting after header */
    size_t offset = sizeof(header);
    int var_count = 0;

    printf("=== EFI Variables ===\n\n");

    while (offset + 16 < file_size) {
        /* Read entry header fields */
        uint32_t length, attr;
        uint64_t time;
        efi_guid_t guid;

        if (offset + 32 > file_size)
            break;

        memcpy(&length, file_data + offset, 4);
        memcpy(&attr, file_data + offset + 4, 4);
        memcpy(&time, file_data + offset + 8, 8);
        memcpy(&guid, file_data + offset + 16, 16);

        /* Validate this looks like a variable entry */
        if (length > file_size - offset - 32 || attr == 0 || attr > 0x80000040) {
            offset++;
            continue;
        }

        /* Find variable name (UTF-16LE after GUID) */
        const uint16_t *name = (const uint16_t *)(file_data + offset + 32);
        size_t name_len = 0;

        while (name_len < 200) {
            if (offset + 32 + (name_len + 1) * 2 >= file_size)
                break;
            if (name[name_len] == 0)
                break;
            /* Check if it's likely a valid ASCII character */
            if (name[name_len] >= 0x80 && name[name_len] != 0)
                break;
            name_len++;
        }

        if (name_len == 0) {
            offset++;
            continue;
        }

        /* Found a valid variable! */
        /* NOTE: 'length' field stores DATA length only, not total entry length */
        size_t name_bytes = (name_len + 1) * 2;
        size_t data_size = length;
        size_t entry_size_unpadded = 32 + name_bytes + data_size;
        size_t entry_size = entry_size_unpadded;

        /* Entries are padded to 8-byte alignment */
        if (entry_size % 8 != 0) {
            entry_size += 8 - (entry_size % 8);
        }

        printf("Variable #%d:\n", ++var_count);
        printf("  Name:       %s\n", utf16le_to_ascii(name, name_len));
        printf("  GUID:       ");
        print_guid_with_name(&guid);
        printf("\n");
        printf("  Attributes: ");
        print_attributes(attr);
        printf("\n");
        printf("  Time:       %llu\n", (unsigned long long)time);
        printf("  Data Size:  %zu bytes (length field: %u)\n", data_size, length);
        printf("  Entry Size: %zu bytes\n", entry_size);
        printf("  Offset:     0x%zx (%zu)\n", offset, offset);

        if (data_size > 0) {
            size_t data_offset = offset + 32 + name_bytes;
            if (data_offset + data_size <= file_size) {
                size_t bytes_to_show = data_size < 32 ? data_size : 32;
                printf("  Data (hex): ");
                for (size_t i = 0; i < bytes_to_show; i++) {
                    printf("%02x ", file_data[data_offset + i]);
                    if (i == 15) printf("\n              ");
                }
                if (data_size > 32) {
                    printf("... (%zu more bytes)", data_size - 32);
                }
                printf("\n");
            }
        }
        printf("\n");

        /* Move to next entry */
        offset += entry_size;
    }

    printf("Total variables found: %d\n", var_count);

cleanup:
    free(file_data);
    return ret;
}
