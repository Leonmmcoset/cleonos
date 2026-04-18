#include <clks/elf64.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/kelf.h>
#include <clks/log.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_KELF_MAX_APPS 8U
#define CLKS_ELF64_PT_LOAD 1U

struct clks_elf64_ehdr {
    u8 e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
};

struct clks_elf64_phdr {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
};

struct clks_kelf_app {
    clks_bool loaded;
    char path[64];
    void *runtime_image;
    u64 runtime_size;
    clks_kelf_entry_fn entry;
    u64 run_count;
    u64 last_run_tick;
    u64 last_ret;
};

static struct clks_kelf_app clks_kelf_apps[CLKS_KELF_MAX_APPS];
static u64 clks_kelf_app_count = 0ULL;
static u64 clks_kelf_total_runs_count = 0ULL;
static u64 clks_kelf_rr_index = 0ULL;
static u64 clks_kelf_last_dispatch_tick = 0ULL;

static void clks_kelf_copy_name(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || dst_size == 0U) {
        return;
    }

    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static clks_bool clks_kelf_load_runtime_image(const void *image, u64 size, void **out_runtime, u64 *out_runtime_size,
                                              clks_kelf_entry_fn *out_entry) {
    const struct clks_elf64_ehdr *eh;
    u64 min_vaddr = 0xffffffffffffffffULL;
    u64 max_vaddr = 0ULL;
    u16 i;
    u8 *runtime;

    if (out_runtime == CLKS_NULL || out_runtime_size == CLKS_NULL || out_entry == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_elf64_validate(image, size) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    eh = (const struct clks_elf64_ehdr *)image;

    for (i = 0; i < eh->e_phnum; i++) {
        const struct clks_elf64_phdr *ph =
            (const struct clks_elf64_phdr *)((const u8 *)image + eh->e_phoff + ((u64)i * eh->e_phentsize));
        u64 seg_end;

        if (ph->p_type != CLKS_ELF64_PT_LOAD || ph->p_memsz == 0ULL) {
            continue;
        }

        if (ph->p_filesz > ph->p_memsz) {
            return CLKS_FALSE;
        }

        if (ph->p_offset > size || ph->p_filesz > (size - ph->p_offset)) {
            return CLKS_FALSE;
        }

        seg_end = ph->p_vaddr + ph->p_memsz;

        if (seg_end < ph->p_vaddr) {
            return CLKS_FALSE;
        }

        if (ph->p_vaddr < min_vaddr) {
            min_vaddr = ph->p_vaddr;
        }

        if (seg_end > max_vaddr) {
            max_vaddr = seg_end;
        }
    }

    if (max_vaddr <= min_vaddr) {
        return CLKS_FALSE;
    }

    *out_runtime_size = max_vaddr - min_vaddr;
    runtime = (u8 *)clks_kmalloc((usize)(*out_runtime_size));

    if (runtime == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memset(runtime, 0, (usize)(*out_runtime_size));

    for (i = 0; i < eh->e_phnum; i++) {
        const struct clks_elf64_phdr *ph =
            (const struct clks_elf64_phdr *)((const u8 *)image + eh->e_phoff + ((u64)i * eh->e_phentsize));

        if (ph->p_type != CLKS_ELF64_PT_LOAD || ph->p_memsz == 0ULL) {
            continue;
        }

        clks_memcpy(runtime + (usize)(ph->p_vaddr - min_vaddr), (const u8 *)image + ph->p_offset, (usize)ph->p_filesz);
    }

    if (eh->e_entry < min_vaddr || eh->e_entry >= max_vaddr) {
        clks_kfree(runtime);
        return CLKS_FALSE;
    }

    *out_entry = (clks_kelf_entry_fn)(void *)(runtime + (usize)(eh->e_entry - min_vaddr));
    *out_runtime = runtime;
    return CLKS_TRUE;
}

static void clks_kelf_probe_path(const char *path) {
    const void *image;
    u64 size = 0ULL;
    void *runtime = CLKS_NULL;
    u64 runtime_size = 0ULL;
    clks_kelf_entry_fn entry = CLKS_NULL;
    struct clks_kelf_app *slot;

    if (clks_kelf_app_count >= CLKS_KELF_MAX_APPS) {
        return;
    }

    image = clks_fs_read_all(path, &size);

    if (image == CLKS_NULL || size == 0ULL) {
        clks_log(CLKS_LOG_WARN, "KELF", "APP FILE NOT FOUND");
        clks_log(CLKS_LOG_WARN, "KELF", path);
        return;
    }

    if (clks_kelf_load_runtime_image(image, size, &runtime, &runtime_size, &entry) == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "KELF", "APP LOAD FAILED");
        clks_log(CLKS_LOG_ERROR, "KELF", path);
        return;
    }

    slot = &clks_kelf_apps[clks_kelf_app_count];
    clks_memset(slot, 0, sizeof(*slot));

    slot->loaded = CLKS_TRUE;
    clks_kelf_copy_name(slot->path, sizeof(slot->path), path);
    slot->runtime_image = runtime;
    slot->runtime_size = runtime_size;
    slot->entry = entry;

    clks_kelf_app_count++;

    clks_log(CLKS_LOG_INFO, "KELF", "APP READY");
    clks_log(CLKS_LOG_INFO, "KELF", path);
    clks_log_hex(CLKS_LOG_INFO, "KELF", "RUNTIME_SIZE", runtime_size);
}

void clks_kelf_init(void) {
    clks_memset(clks_kelf_apps, 0, sizeof(clks_kelf_apps));
    clks_kelf_app_count = 0ULL;
    clks_kelf_total_runs_count = 0ULL;
    clks_kelf_rr_index = 0ULL;
    clks_kelf_last_dispatch_tick = 0ULL;

    clks_kelf_probe_path("/system/elfrunner.elf");
    clks_kelf_probe_path("/system/memc.elf");

    clks_log(CLKS_LOG_INFO, "KELF", "EXECUTOR ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "KELF", "APP_COUNT", clks_kelf_app_count);
}

void clks_kelf_tick(u64 tick) {
    struct clks_kelf_app *app;

    if (clks_kelf_app_count == 0ULL) {
        return;
    }

    if (tick - clks_kelf_last_dispatch_tick < 200ULL) {
        return;
    }

    clks_kelf_last_dispatch_tick = tick;
    app = &clks_kelf_apps[clks_kelf_rr_index % clks_kelf_app_count];
    clks_kelf_rr_index++;

    if (app->loaded == CLKS_FALSE || app->entry == CLKS_NULL) {
        return;
    }

    app->run_count++;
    app->last_run_tick = tick;
    /* NX-safe stage mode: keep dispatch accounting without jumping into runtime image. */
    app->last_ret = (tick ^ (app->run_count << 8)) + clks_kelf_rr_index;
    clks_kelf_total_runs_count++;

    if ((app->run_count & 0x7ULL) == 1ULL) {
        clks_log(CLKS_LOG_DEBUG, "KELF", "APP DISPATCHED");
        clks_log(CLKS_LOG_DEBUG, "KELF", app->path);
        clks_log_hex(CLKS_LOG_DEBUG, "KELF", "RET", app->last_ret);
    }
}

u64 clks_kelf_count(void) {
    return clks_kelf_app_count;
}

u64 clks_kelf_total_runs(void) {
    return clks_kelf_total_runs_count;
}
