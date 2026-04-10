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

    if (out_loaded == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memset(out_loaded, 0, sizeof(*out_loaded));

    if (clks_elf64_validate(image, size) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    eh = (const struct clks_elf64_ehdr *)image;
    out_loaded->entry = eh->e_entry;

    for (i = 0; i < eh->e_phnum; i++) {
        const struct clks_elf64_phdr *ph =
            (const struct clks_elf64_phdr *)((const u8 *)image + eh->e_phoff + ((u64)i * eh->e_phentsize));
        void *dst;

        if (ph->p_type != CLKS_ELF64_PT_LOAD || ph->p_memsz == 0ULL) {
            continue;
        }

        if (out_loaded->segment_count >= CLKS_ELF64_MAX_SEGMENTS) {
            return CLKS_FALSE;
        }

        dst = clks_kmalloc((usize)ph->p_memsz);

        if (dst == CLKS_NULL) {
            return CLKS_FALSE;
        }

        clks_memset(dst, 0, (usize)ph->p_memsz);
        clks_memcpy(dst, (const void *)((const u8 *)image + ph->p_offset), (usize)ph->p_filesz);

        out_loaded->segments[out_loaded->segment_count].base = dst;
        out_loaded->segments[out_loaded->segment_count].vaddr = ph->p_vaddr;
        out_loaded->segments[out_loaded->segment_count].memsz = ph->p_memsz;
        out_loaded->segments[out_loaded->segment_count].filesz = ph->p_filesz;
        out_loaded->segments[out_loaded->segment_count].flags = ph->p_flags;
        out_loaded->segment_count++;
    }

    return CLKS_TRUE;
}