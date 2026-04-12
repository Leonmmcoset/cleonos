#ifndef CLKS_ELF64_H
#define CLKS_ELF64_H

#include <clks/types.h>

#define CLKS_ELF64_MAX_SEGMENTS 16U

#define CLKS_ELF64_PT_LOAD 1U

struct clks_elf64_info {
    u64 entry;
    u16 phnum;
    u16 loadable_segments;
    u64 total_load_memsz;
};

struct clks_elf64_loaded_segment {
    void *base;
    u64 vaddr;
    u64 memsz;
    u64 filesz;
    u32 flags;
};

struct clks_elf64_loaded_image {
    u64 entry;
    void *image_base;
    u64 image_size;
    u64 image_vaddr_base;
    u16 segment_count;
    struct clks_elf64_loaded_segment segments[CLKS_ELF64_MAX_SEGMENTS];
};

clks_bool clks_elf64_validate(const void *image, u64 size);
clks_bool clks_elf64_inspect(const void *image, u64 size, struct clks_elf64_info *out_info);
clks_bool clks_elf64_load(const void *image, u64 size, struct clks_elf64_loaded_image *out_loaded);
void clks_elf64_unload(struct clks_elf64_loaded_image *loaded);
void *clks_elf64_entry_pointer(const struct clks_elf64_loaded_image *loaded, u64 entry);

#endif
