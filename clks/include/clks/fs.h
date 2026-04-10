#ifndef CLKS_FS_H
#define CLKS_FS_H

#include <clks/types.h>

enum clks_fs_node_type {
    CLKS_FS_NODE_FILE = 1,
    CLKS_FS_NODE_DIR = 2,
};

struct clks_fs_node_info {
    enum clks_fs_node_type type;
    u64 size;
};

void clks_fs_init(void);
clks_bool clks_fs_is_ready(void);
clks_bool clks_fs_stat(const char *path, struct clks_fs_node_info *out_info);
const void *clks_fs_read_all(const char *path, u64 *out_size);
u64 clks_fs_count_children(const char *dir_path);
clks_bool clks_fs_get_child_name(const char *dir_path, u64 index, char *out_name, usize out_name_size);
u64 clks_fs_node_count(void);

#endif
