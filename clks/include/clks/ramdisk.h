#ifndef CLKS_RAMDISK_H
#define CLKS_RAMDISK_H

#include <clks/types.h>

#define CLKS_RAMDISK_PATH_MAX 192U

enum clks_ramdisk_entry_type {
    CLKS_RAMDISK_ENTRY_FILE = 1,
    CLKS_RAMDISK_ENTRY_DIR = 2,
};

struct clks_ramdisk_entry {
    enum clks_ramdisk_entry_type type;
    char path[CLKS_RAMDISK_PATH_MAX];
    const void *data;
    u64 size;
};

typedef clks_bool (*clks_ramdisk_iter_fn)(const struct clks_ramdisk_entry *entry, void *ctx);

clks_bool clks_ramdisk_iterate(const void *image, u64 image_size, clks_ramdisk_iter_fn iter_fn, void *ctx);

#endif
