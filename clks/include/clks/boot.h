#ifndef CLKS_BOOT_H
#define CLKS_BOOT_H

#include <clks/limine.h>
#include <clks/types.h>

clks_bool clks_boot_base_revision_supported(void);
const struct limine_framebuffer *clks_boot_get_framebuffer(void);
const struct limine_memmap_response *clks_boot_get_memmap(void);
const struct limine_file *clks_boot_get_executable_file(void);
u64 clks_boot_get_module_count(void);
const struct limine_file *clks_boot_get_module(u64 index);

#endif
