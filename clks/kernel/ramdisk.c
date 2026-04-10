#include <clks/compiler.h>
#include <clks/ramdisk.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_TAR_BLOCK_SIZE 512ULL

struct clks_tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} CLKS_PACKED;

static clks_bool clks_ramdisk_is_zero_block(const u8 *block) {
    u64 i;

    for (i = 0; i < CLKS_TAR_BLOCK_SIZE; i++) {
        if (block[i] != 0U) {
            return CLKS_FALSE;
        }
    }

    return CLKS_TRUE;
}

static usize clks_ramdisk_field_len(const char *field, usize max_len) {
    usize i = 0;

    while (i < max_len && field[i] != '\0') {
        i++;
    }

    return i;
}

static clks_bool clks_ramdisk_parse_octal_u64(const char *field, usize len, u64 *out_value) {
    usize i = 0;
    u64 value = 0ULL;

    if (out_value == CLKS_NULL) {
        return CLKS_FALSE;
    }

    while (i < len && (field[i] == ' ' || field[i] == '\0')) {
        i++;
    }

    for (; i < len; i++) {
        char ch = field[i];

        if (ch == ' ' || ch == '\0') {
            break;
        }

        if (ch < '0' || ch > '7') {
            return CLKS_FALSE;
        }

        if (value > (0xffffffffffffffffULL >> 3)) {
            return CLKS_FALSE;
        }

        value = (value << 3) + (u64)(ch - '0');
    }

    *out_value = value;
    return CLKS_TRUE;
}

static clks_bool clks_ramdisk_build_path(const struct clks_tar_header *hdr, char *out_path, usize out_path_size) {
    char raw[CLKS_RAMDISK_PATH_MAX];
    usize prefix_len;
    usize name_len;
    usize cursor = 0;
    usize read_pos = 0;
    usize out_cursor = 0;

    if (hdr == CLKS_NULL || out_path == CLKS_NULL || out_path_size == 0U) {
        return CLKS_FALSE;
    }

    raw[0] = '\0';
    out_path[0] = '\0';

    prefix_len = clks_ramdisk_field_len(hdr->prefix, sizeof(hdr->prefix));
    name_len = clks_ramdisk_field_len(hdr->name, sizeof(hdr->name));

    if (name_len == 0U) {
        return CLKS_FALSE;
    }

    if (prefix_len != 0U) {
        if (prefix_len + 1U >= sizeof(raw)) {
            return CLKS_FALSE;
        }

        clks_memcpy(raw, hdr->prefix, prefix_len);
        cursor = prefix_len;
        raw[cursor++] = '/';
    }

    if (cursor + name_len >= sizeof(raw)) {
        return CLKS_FALSE;
    }

    clks_memcpy(raw + cursor, hdr->name, name_len);
    cursor += name_len;
    raw[cursor] = '\0';

    while ((raw[read_pos] == '.' && raw[read_pos + 1U] == '/') || raw[read_pos] == '/') {
        if (raw[read_pos] == '.') {
            read_pos += 2U;
        } else {
            read_pos++;
        }
    }

    while (raw[read_pos] != '\0' && out_cursor + 1U < out_path_size) {
        out_path[out_cursor++] = raw[read_pos++];
    }

    if (raw[read_pos] != '\0') {
        return CLKS_FALSE;
    }

    while (out_cursor > 0U && out_path[out_cursor - 1U] == '/') {
        out_cursor--;
    }

    out_path[out_cursor] = '\0';

    if (out_cursor == 0U) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

clks_bool clks_ramdisk_iterate(const void *image, u64 image_size, clks_ramdisk_iter_fn iter_fn, void *ctx) {
    const u8 *bytes = (const u8 *)image;
    u64 offset = 0ULL;

    if (image == CLKS_NULL || iter_fn == CLKS_NULL) {
        return CLKS_FALSE;
    }

    while (offset + CLKS_TAR_BLOCK_SIZE <= image_size) {
        const struct clks_tar_header *hdr;
        u64 payload_offset;
        u64 file_size;
        u64 aligned_size;
        struct clks_ramdisk_entry entry;
        clks_bool emit = CLKS_FALSE;

        hdr = (const struct clks_tar_header *)(bytes + offset);

        if (clks_ramdisk_is_zero_block((const u8 *)hdr) == CLKS_TRUE) {
            break;
        }

        if (clks_ramdisk_parse_octal_u64(hdr->size, sizeof(hdr->size), &file_size) == CLKS_FALSE) {
            return CLKS_FALSE;
        }

        payload_offset = offset + CLKS_TAR_BLOCK_SIZE;

        if (payload_offset > image_size) {
            return CLKS_FALSE;
        }

        if (file_size > (image_size - payload_offset)) {
            return CLKS_FALSE;
        }

        clks_memset(&entry, 0, sizeof(entry));

        if (clks_ramdisk_build_path(hdr, entry.path, sizeof(entry.path)) == CLKS_TRUE) {
            if (hdr->typeflag == '5') {
                entry.type = CLKS_RAMDISK_ENTRY_DIR;
                entry.data = CLKS_NULL;
                entry.size = 0ULL;
                emit = CLKS_TRUE;
            } else if (hdr->typeflag == '\0' || hdr->typeflag == '0') {
                entry.type = CLKS_RAMDISK_ENTRY_FILE;
                entry.data = (const void *)(bytes + payload_offset);
                entry.size = file_size;
                emit = CLKS_TRUE;
            }
        }

        if (emit == CLKS_TRUE) {
            if (iter_fn(&entry, ctx) == CLKS_FALSE) {
                return CLKS_FALSE;
            }
        }

        aligned_size = (file_size + (CLKS_TAR_BLOCK_SIZE - 1ULL)) & ~(CLKS_TAR_BLOCK_SIZE - 1ULL);

        if (payload_offset + aligned_size < payload_offset) {
            return CLKS_FALSE;
        }

        offset = payload_offset + aligned_size;
    }

    return CLKS_TRUE;
}
