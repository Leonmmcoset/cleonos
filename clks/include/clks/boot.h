#ifndef CLKS_BOOT_H
#define CLKS_BOOT_H

#include <clks/limine.h>
#include <clks/types.h>

clks_bool clks_boot_base_revision_supported(void);
const struct limine_framebuffer *clks_boot_get_framebuffer(void);

#endif