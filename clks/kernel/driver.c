#include <clks/driver.h>
#include <clks/audio.h>
#include <clks/elf64.h>
#include <clks/framebuffer.h>
#include <clks/fs.h>
#include <clks/log.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_DRIVER_MAX 32U
#define CLKS_DRIVER_CHILD_NAME_MAX 96U
#define CLKS_DRIVER_PATH_MAX 224U

static struct clks_driver_info clks_driver_table[CLKS_DRIVER_MAX];
static u64 clks_driver_table_count = 0ULL;
static u64 clks_driver_table_elf_count = 0ULL;

static void clks_driver_copy_name(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || dst_size == 0U) {
        return;
    }

    if (src == CLKS_NULL) {
        dst[0] = '\0';
        return;
    }

    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static clks_bool clks_driver_has_elf_suffix(const char *name) {
    usize len;

    if (name == CLKS_NULL) {
        return CLKS_FALSE;
    }

    len = clks_strlen(name);

    if (len < 4U) {
        return CLKS_FALSE;
    }

    return (name[len - 4U] == '.' && name[len - 3U] == 'e' && name[len - 2U] == 'l' && name[len - 1U] == 'f')
               ? CLKS_TRUE
               : CLKS_FALSE;
}

static clks_bool clks_driver_build_path(const char *child_name, char *out_path, usize out_size) {
    static const char prefix[] = "/driver/";
    usize prefix_len = sizeof(prefix) - 1U;
    usize child_len;

    if (child_name == CLKS_NULL || out_path == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    child_len = clks_strlen(child_name);

    if (prefix_len + child_len + 1U > out_size) {
        return CLKS_FALSE;
    }

    clks_memcpy(out_path, prefix, prefix_len);
    clks_memcpy(out_path + prefix_len, child_name, child_len);
    out_path[prefix_len + child_len] = '\0';
    return CLKS_TRUE;
}

static clks_bool clks_driver_push(const char *name, enum clks_driver_kind kind, enum clks_driver_state state,
                                  clks_bool from_elf, u64 image_size, u64 elf_entry) {
    struct clks_driver_info *slot;

    if (clks_driver_table_count >= CLKS_DRIVER_MAX) {
        return CLKS_FALSE;
    }

    slot = &clks_driver_table[clks_driver_table_count];
    clks_memset(slot, 0, sizeof(*slot));

    clks_driver_copy_name(slot->name, sizeof(slot->name), name);
    slot->kind = kind;
    slot->state = state;
    slot->from_elf = from_elf;
    slot->image_size = image_size;
    slot->elf_entry = elf_entry;

    clks_driver_table_count++;

    if (from_elf == CLKS_TRUE) {
        clks_driver_table_elf_count++;
    }

    return CLKS_TRUE;
}

static void clks_driver_register_builtins(void) {
    clks_driver_push("serial", CLKS_DRIVER_KIND_BUILTIN_CHAR, CLKS_DRIVER_STATE_READY, CLKS_FALSE, 0ULL, 0ULL);

    if (clks_fb_ready() == CLKS_TRUE) {
        clks_driver_push("framebuffer", CLKS_DRIVER_KIND_BUILTIN_VIDEO, CLKS_DRIVER_STATE_READY, CLKS_FALSE, 0ULL,
                         0ULL);
        clks_driver_push("tty", CLKS_DRIVER_KIND_BUILTIN_TTY, CLKS_DRIVER_STATE_READY, CLKS_FALSE, 0ULL, 0ULL);
    } else {
        clks_driver_push("framebuffer", CLKS_DRIVER_KIND_BUILTIN_VIDEO, CLKS_DRIVER_STATE_FAILED, CLKS_FALSE, 0ULL,
                         0ULL);
        clks_driver_push("tty", CLKS_DRIVER_KIND_BUILTIN_TTY, CLKS_DRIVER_STATE_FAILED, CLKS_FALSE, 0ULL, 0ULL);
    }

    if (clks_audio_available() == CLKS_TRUE) {
        clks_driver_push("pcspeaker", CLKS_DRIVER_KIND_BUILTIN_AUDIO, CLKS_DRIVER_STATE_READY, CLKS_FALSE, 0ULL, 0ULL);
    } else {
        clks_driver_push("pcspeaker", CLKS_DRIVER_KIND_BUILTIN_AUDIO, CLKS_DRIVER_STATE_FAILED, CLKS_FALSE, 0ULL, 0ULL);
    }
}

static void clks_driver_probe_driver_dir(void) {
    u64 child_count;
    u64 i;

    if (clks_fs_is_ready() == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "DRV", "FS NOT READY FOR DRIVER PROBE");
        return;
    }

    child_count = clks_fs_count_children("/driver");

    for (i = 0ULL; i < child_count; i++) {
        char child_name[CLKS_DRIVER_CHILD_NAME_MAX];
        char full_path[CLKS_DRIVER_PATH_MAX];
        const void *image;
        u64 image_size = 0ULL;
        struct clks_elf64_info info;

        clks_memset(child_name, 0, sizeof(child_name));
        clks_memset(full_path, 0, sizeof(full_path));

        if (clks_fs_get_child_name("/driver", i, child_name, sizeof(child_name)) == CLKS_FALSE) {
            continue;
        }

        if (clks_driver_has_elf_suffix(child_name) == CLKS_FALSE) {
            continue;
        }

        if (clks_driver_build_path(child_name, full_path, sizeof(full_path)) == CLKS_FALSE) {
            clks_driver_push(child_name, CLKS_DRIVER_KIND_ELF, CLKS_DRIVER_STATE_FAILED, CLKS_TRUE, 0ULL, 0ULL);
            continue;
        }

        image = clks_fs_read_all(full_path, &image_size);

        if (image == CLKS_NULL) {
            clks_log(CLKS_LOG_ERROR, "DRV", "DRIVER ELF MISSING");
            clks_log(CLKS_LOG_ERROR, "DRV", full_path);
            clks_driver_push(child_name, CLKS_DRIVER_KIND_ELF, CLKS_DRIVER_STATE_FAILED, CLKS_TRUE, 0ULL, 0ULL);
            continue;
        }

        if (clks_elf64_inspect(image, image_size, &info) == CLKS_FALSE) {
            clks_log(CLKS_LOG_ERROR, "DRV", "DRIVER ELF INVALID");
            clks_log(CLKS_LOG_ERROR, "DRV", full_path);
            clks_driver_push(child_name, CLKS_DRIVER_KIND_ELF, CLKS_DRIVER_STATE_FAILED, CLKS_TRUE, image_size, 0ULL);
            continue;
        }

        clks_log(CLKS_LOG_INFO, "DRV", "DRIVER ELF READY");
        clks_log(CLKS_LOG_INFO, "DRV", full_path);
        clks_log_hex(CLKS_LOG_INFO, "DRV", "ENTRY", info.entry);

        clks_driver_push(child_name, CLKS_DRIVER_KIND_ELF, CLKS_DRIVER_STATE_READY, CLKS_TRUE, image_size, info.entry);
    }
}

void clks_driver_init(void) {
    clks_memset(clks_driver_table, 0, sizeof(clks_driver_table));
    clks_driver_table_count = 0ULL;
    clks_driver_table_elf_count = 0ULL;

    clks_driver_register_builtins();
    clks_driver_probe_driver_dir();

    clks_log(CLKS_LOG_INFO, "DRV", "DRIVER MANAGER ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "DRV", "REGISTERED", clks_driver_table_count);
    clks_log_hex(CLKS_LOG_INFO, "DRV", "ELF_DRIVERS", clks_driver_table_elf_count);
}

u64 clks_driver_count(void) {
    return clks_driver_table_count;
}

u64 clks_driver_elf_count(void) {
    return clks_driver_table_elf_count;
}

clks_bool clks_driver_get(u64 index, struct clks_driver_info *out_info) {
    if (out_info == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (index >= clks_driver_table_count) {
        return CLKS_FALSE;
    }

    *out_info = clks_driver_table[index];
    return CLKS_TRUE;
}
