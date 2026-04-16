#ifndef CLKS_DRIVER_H
#define CLKS_DRIVER_H

#include <clks/types.h>

#define CLKS_DRIVER_NAME_MAX 32U

enum clks_driver_kind {
    CLKS_DRIVER_KIND_BUILTIN_CHAR = 1,
    CLKS_DRIVER_KIND_BUILTIN_VIDEO = 2,
    CLKS_DRIVER_KIND_BUILTIN_TTY = 3,
    CLKS_DRIVER_KIND_ELF = 4,
    CLKS_DRIVER_KIND_BUILTIN_AUDIO = 5,
};

enum clks_driver_state {
    CLKS_DRIVER_STATE_OFFLINE = 0,
    CLKS_DRIVER_STATE_READY = 1,
    CLKS_DRIVER_STATE_FAILED = 2,
};

struct clks_driver_info {
    char name[CLKS_DRIVER_NAME_MAX];
    enum clks_driver_kind kind;
    enum clks_driver_state state;
    clks_bool from_elf;
    u64 image_size;
    u64 elf_entry;
};

void clks_driver_init(void);
u64 clks_driver_count(void);
u64 clks_driver_elf_count(void);
clks_bool clks_driver_get(u64 index, struct clks_driver_info *out_info);

#endif
