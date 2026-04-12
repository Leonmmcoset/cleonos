#include <clks/elf64.h>
#include <clks/heap.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_ELF64_MAGIC_0 0x7FU
#define CLKS_ELF64_MAGIC_1 'E'
#define CLKS_ELF64_MAGIC_2 'L'
#define CLKS_ELF64_MAGIC_3 'F'

#define CLKS_ELF64_CLASS_64 2U
#define CLKS_ELF64_DATA_LSB 1U
#define CLKS_ELF64_VERSION  1U

#define CLKS_ELF64_ET_EXEC 2U
#define CLKS_ELF64_ET_DYN  3U

#define CLKS_ELF64_EM_X86_64 62U

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

static clks_bool clks_elf64_header_ok(const struct clks_elf64_ehdr *eh) {
    if (eh->e_ident[0] != CLKS_ELF64_MAGIC_0 ||
        eh->e_ident[1] != CLKS_ELF64_MAGIC_1 ||
        eh->e_ident[2] != CLKS_ELF64_MAGIC_2 ||
        eh->e_ident[3] != CLKS_ELF64_MAGIC_3) {
        return CLKS_FALSE;
    }

    if (eh->e_ident[4] != CLKS_ELF64_CLASS_64 || eh->e_ident[5] != CLKS_ELF64_DATA_LSB) {
        return CLKS_FALSE;
    }

    if (eh->e_ident[6] != CLKS_ELF64_VERSION || eh->e_version != CLKS_ELF64_VERSION) {
        return CLKS_FALSE;
    }

    if (eh->e_type != CLKS_ELF64_ET_EXEC && eh->e_type != CLKS_ELF64_ET_DYN) {
        return CLKS_FALSE;
    }

    if (eh->e_machine != CLKS_ELF64_EM_X86_64) {
        return CLKS_FALSE;
    }

    if (eh->e_ehsize != sizeof(struct clks_elf64_ehdr)) {
        return CLKS_FALSE;
    }

    if (eh->e_phentsize != sizeof(struct clks_elf64_phdr)) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_elf64_range_ok(u64 off, u64 len, u64 total) {
    if (off > total) {
        return CLKS_FALSE;
    }

    if (len > (total - off)) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

clks_bool clks_elf64_validate(const void *image, u64 size) {
    const struct clks_elf64_ehdr *eh;
    u64 ph_table_size;
    u16 i;

    if (image == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (size < sizeof(struct clks_elf64_ehdr)) {
        return CLKS_FALSE;
    }

    eh = (const struct clks_elf64_ehdr *)image;

    if (clks_elf64_header_ok(eh) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    ph_table_size = (u64)eh->e_phnum * (u64)eh->e_phentsize;

    if (clks_elf64_range_ok(eh->e_phoff, ph_table_size, size) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    for (i = 0; i < eh->e_phnum; i++) {
        const struct clks_elf64_phdr *ph =
            (const struct clks_elf64_phdr *)((const u8 *)image + eh->e_phoff + ((u64)i * eh->e_phentsize));

        if (ph->p_type != CLKS_ELF64_PT_LOAD) {
            continue;
        }

        if (ph->p_filesz > ph->p_memsz) {
            return CLKS_FALSE;
        }

        if (clks_elf64_range_ok(ph->p_offset, ph->p_filesz, size) == CLKS_FALSE) {
            return CLKS_FALSE;
        }
    }

    return CLKS_TRUE;
}

clks_bool clks_elf64_inspect(const void *image, u64 size, struct clks_elf64_info *out_info) {
    const struct clks_elf64_ehdr *eh;
    u16 i;

    if (out_info == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memset(out_info, 0, sizeof(*out_info));

    if (clks_elf64_validate(image, size) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    eh = (const struct clks_elf64_ehdr *)image;

    out_info->entry = eh->e_entry;
    out_info->phnum = eh->e_phnum;

    for (i = 0; i < eh->e_phnum; i++) {
        const struct clks_elf64_phdr *ph =
            (const struct clks_elf64_phdr *)((const u8 *)image + eh->e_phoff + ((u64)i * eh->e_phentsize));

        if (ph->p_type != CLKS_ELF64_PT_LOAD) {
            continue;
        }

        out_info->loadable_segments++;
        out_info->total_load_memsz += ph->p_memsz;
    }

    return CLKS_TRUE;
}

clks_bool clks_elf64_load(const void *image, u64 size, struct clks_elf64_loaded_image *out_loaded) {
    const struct clks_elf64_ehdr *eh;
    u16 i;
    u16 load_count = 0U;
    u64 min_vaddr = 0ULL;
    u64 max_vaddr_end = 0ULL;
    u64 span;
    void *image_base;

    if (out_loaded == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memset(out_loaded, 0, sizeof(*out_loaded));

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

        if (load_count == 0U || ph->p_vaddr < min_vaddr) {
            min_vaddr = ph->p_vaddr;
        }

        seg_end = ph->p_vaddr + ph->p_memsz;
        if (seg_end < ph->p_vaddr) {
            return CLKS_FALSE;
        }

        if (load_count == 0U || seg_end > max_vaddr_end) {
            max_vaddr_end = seg_end;
        }

        load_count++;
    }

    if (load_count == 0U) {
        return CLKS_FALSE;
    }

    if (load_count > CLKS_ELF64_MAX_SEGMENTS) {
        return CLKS_FALSE;
    }

    span = max_vaddr_end - min_vaddr;
    if (span == 0ULL) {
        return CLKS_FALSE;
    }

    image_base = clks_kmalloc((usize)span);
    if (image_base == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memset(image_base, 0, (usize)span);

    out_loaded->entry = eh->e_entry;
    out_loaded->image_base = image_base;
    out_loaded->image_size = span;
    out_loaded->image_vaddr_base = min_vaddr;
    out_loaded->segment_count = 0U;

    for (i = 0; i < eh->e_phnum; i++) {
        const struct clks_elf64_phdr *ph =
            (const struct clks_elf64_phdr *)((const u8 *)image + eh->e_phoff + ((u64)i * eh->e_phentsize));
        u64 seg_off;
        u8 *seg_dst;

        if (ph->p_type != CLKS_ELF64_PT_LOAD || ph->p_memsz == 0ULL) {
            continue;
        }

        if (out_loaded->segment_count >= CLKS_ELF64_MAX_SEGMENTS) {
            clks_elf64_unload(out_loaded);
            return CLKS_FALSE;
        }

        seg_off = ph->p_vaddr - min_vaddr;
        if (seg_off > span || ph->p_memsz > (span - seg_off)) {
            clks_elf64_unload(out_loaded);
            return CLKS_FALSE;
        }

        seg_dst = (u8 *)image_base + (usize)seg_off;
        clks_memcpy(seg_dst, (const void *)((const u8 *)image + ph->p_offset), (usize)ph->p_filesz);

        out_loaded->segments[out_loaded->segment_count].base = seg_dst;
        out_loaded->segments[out_loaded->segment_count].vaddr = ph->p_vaddr;
        out_loaded->segments[out_loaded->segment_count].memsz = ph->p_memsz;
        out_loaded->segments[out_loaded->segment_count].filesz = ph->p_filesz;
        out_loaded->segments[out_loaded->segment_count].flags = ph->p_flags;
        out_loaded->segment_count++;
    }

    return CLKS_TRUE;
}

void clks_elf64_unload(struct clks_elf64_loaded_image *loaded) {
    if (loaded == CLKS_NULL) {
        return;
    }

    if (loaded->image_base != CLKS_NULL) {
        clks_kfree(loaded->image_base);
    }

    clks_memset(loaded, 0, sizeof(*loaded));
}

void *clks_elf64_entry_pointer(const struct clks_elf64_loaded_image *loaded, u64 entry) {
    u64 off;

    if (loaded == CLKS_NULL || loaded->image_base == CLKS_NULL) {
        return CLKS_NULL;
    }

    if (entry < loaded->image_vaddr_base) {
        return CLKS_NULL;
    }

    off = entry - loaded->image_vaddr_base;

    if (off >= loaded->image_size) {
        return CLKS_NULL;
    }

    return (void *)((u8 *)loaded->image_base + (usize)off);
}
