#include <clks/boot.h>
#include <clks/fs.h>
#include <clks/log.h>
#include <clks/ramdisk.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_FS_MAX_NODES 512U
#define CLKS_FS_PATH_MAX  CLKS_RAMDISK_PATH_MAX

struct clks_fs_node {
    clks_bool used;
    enum clks_fs_node_type type;
    u16 parent;
    u16 reserved;
    const void *data;
    u64 size;
    char path[CLKS_FS_PATH_MAX];
};

struct clks_fs_build_stats {
    u64 file_count;
    u64 dir_count;
};

static struct clks_fs_node clks_fs_nodes[CLKS_FS_MAX_NODES];
static u16 clks_fs_nodes_used = 0U;
static clks_bool clks_fs_ready = CLKS_FALSE;

static clks_bool clks_fs_normalize_external_path(const char *path, char *out_internal, usize out_size) {
    usize in_pos = 0;
    usize out_pos = 0;

    if (path == CLKS_NULL || out_internal == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    if (path[0] != '/') {
        return CLKS_FALSE;
    }

    while (path[in_pos] == '/') {
        in_pos++;
    }

    while (path[in_pos] != '\0') {
        usize comp_start = in_pos;
        usize comp_len;

        while (path[in_pos] != '\0' && path[in_pos] != '/') {
            in_pos++;
        }

        comp_len = in_pos - comp_start;

        if (comp_len == 0U) {
            while (path[in_pos] == '/') {
                in_pos++;
            }
            continue;
        }

        if (comp_len == 1U && path[comp_start] == '.') {
            while (path[in_pos] == '/') {
                in_pos++;
            }
            continue;
        }

        if (comp_len == 2U && path[comp_start] == '.' && path[comp_start + 1U] == '.') {
            return CLKS_FALSE;
        }

        if (out_pos != 0U) {
            if (out_pos + 1U >= out_size) {
                return CLKS_FALSE;
            }
            out_internal[out_pos++] = '/';
        }

        if (out_pos + comp_len >= out_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_internal + out_pos, path + comp_start, comp_len);
        out_pos += comp_len;

        while (path[in_pos] == '/') {
            in_pos++;
        }
    }

    out_internal[out_pos] = '\0';
    return CLKS_TRUE;
}

static i32 clks_fs_find_node_by_internal(const char *internal_path) {
    u16 i;

    for (i = 0U; i < clks_fs_nodes_used; i++) {
        if (clks_fs_nodes[i].used == CLKS_FALSE) {
            continue;
        }

        if (clks_strcmp(clks_fs_nodes[i].path, internal_path) == 0) {
            return (i32)i;
        }
    }

    return -1;
}

static i32 clks_fs_find_node_by_external(const char *external_path) {
    char internal[CLKS_FS_PATH_MAX];

    if (clks_fs_normalize_external_path(external_path, internal, sizeof(internal)) == CLKS_FALSE) {
        return -1;
    }

    return clks_fs_find_node_by_internal(internal);
}

static const char *clks_fs_basename(const char *internal_path) {
    usize len;
    usize i;

    if (internal_path == CLKS_NULL) {
        return "";
    }

    len = clks_strlen(internal_path);

    if (len == 0U) {
        return "";
    }

    for (i = len; i != 0U; i--) {
        if (internal_path[i - 1U] == '/') {
            return &internal_path[i];
        }
    }

    return internal_path;
}

static clks_bool clks_fs_split_parent(const char *internal_path, char *parent_out, usize parent_out_size) {
    usize len;
    usize i;

    if (internal_path == CLKS_NULL || parent_out == CLKS_NULL || parent_out_size == 0U) {
        return CLKS_FALSE;
    }

    len = clks_strlen(internal_path);

    if (len == 0U) {
        parent_out[0] = '\0';
        return CLKS_TRUE;
    }

    for (i = len; i != 0U; i--) {
        if (internal_path[i - 1U] == '/') {
            usize parent_len = i - 1U;

            if (parent_len >= parent_out_size) {
                return CLKS_FALSE;
            }

            clks_memcpy(parent_out, internal_path, parent_len);
            parent_out[parent_len] = '\0';
            return CLKS_TRUE;
        }
    }

    parent_out[0] = '\0';
    return CLKS_TRUE;
}

static i32 clks_fs_create_or_update_node(const char *internal_path,
                                         enum clks_fs_node_type type,
                                         u16 parent,
                                         const void *data,
                                         u64 size) {
    i32 existing;
    usize path_len;

    if (internal_path == CLKS_NULL) {
        return -1;
    }

    path_len = clks_strlen(internal_path);

    if (path_len >= CLKS_FS_PATH_MAX) {
        return -1;
    }

    existing = clks_fs_find_node_by_internal(internal_path);

    if (existing >= 0) {
        struct clks_fs_node *node = &clks_fs_nodes[(u16)existing];

        if (node->type != type) {
            return -1;
        }

        node->parent = parent;

        if (type == CLKS_FS_NODE_FILE) {
            node->data = data;
            node->size = size;
        }

        return existing;
    }

    if (clks_fs_nodes_used >= CLKS_FS_MAX_NODES) {
        return -1;
    }

    clks_fs_nodes[clks_fs_nodes_used].used = CLKS_TRUE;
    clks_fs_nodes[clks_fs_nodes_used].type = type;
    clks_fs_nodes[clks_fs_nodes_used].parent = parent;
    clks_fs_nodes[clks_fs_nodes_used].reserved = 0U;
    clks_fs_nodes[clks_fs_nodes_used].data = (type == CLKS_FS_NODE_FILE) ? data : CLKS_NULL;
    clks_fs_nodes[clks_fs_nodes_used].size = (type == CLKS_FS_NODE_FILE) ? size : 0ULL;
    clks_memcpy(clks_fs_nodes[clks_fs_nodes_used].path, internal_path, path_len + 1U);

    clks_fs_nodes_used++;
    return (i32)(clks_fs_nodes_used - 1U);
}

static clks_bool clks_fs_ensure_root(void) {
    if (clks_fs_create_or_update_node("", CLKS_FS_NODE_DIR, 0U, CLKS_NULL, 0ULL) != 0) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_fs_ensure_dir_hierarchy(const char *internal_dir_path) {
    char prefix[CLKS_FS_PATH_MAX];
    usize cursor = 0;
    usize i = 0;
    u16 current_parent = 0U;

    prefix[0] = '\0';

    if (internal_dir_path == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (internal_dir_path[0] == '\0') {
        return CLKS_TRUE;
    }

    while (internal_dir_path[i] != '\0') {
        usize comp_start = i;
        usize comp_len;
        i32 node_index;

        while (internal_dir_path[i] != '\0' && internal_dir_path[i] != '/') {
            i++;
        }

        comp_len = i - comp_start;

        if (comp_len == 0U) {
            return CLKS_FALSE;
        }

        if (cursor != 0U) {
            if (cursor + 1U >= sizeof(prefix)) {
                return CLKS_FALSE;
            }
            prefix[cursor++] = '/';
        }

        if (cursor + comp_len >= sizeof(prefix)) {
            return CLKS_FALSE;
        }

        clks_memcpy(prefix + cursor, internal_dir_path + comp_start, comp_len);
        cursor += comp_len;
        prefix[cursor] = '\0';

        node_index = clks_fs_find_node_by_internal(prefix);

        if (node_index < 0) {
            node_index = clks_fs_create_or_update_node(prefix, CLKS_FS_NODE_DIR, current_parent, CLKS_NULL, 0ULL);

            if (node_index < 0) {
                return CLKS_FALSE;
            }
        } else if (clks_fs_nodes[(u16)node_index].type != CLKS_FS_NODE_DIR) {
            return CLKS_FALSE;
        }

        current_parent = (u16)node_index;

        if (internal_dir_path[i] == '/') {
            i++;
        }
    }

    return CLKS_TRUE;
}

static clks_bool clks_fs_require_directory(const char *external_path) {
    i32 node_index = clks_fs_find_node_by_external(external_path);

    if (node_index < 0) {
        clks_log(CLKS_LOG_ERROR, "FS", "MISSING REQUIRED DIRECTORY");
        clks_log(CLKS_LOG_ERROR, "FS", external_path);
        return CLKS_FALSE;
    }

    if (clks_fs_nodes[(u16)node_index].type != CLKS_FS_NODE_DIR) {
        clks_log(CLKS_LOG_ERROR, "FS", "REQUIRED PATH IS NOT DIRECTORY");
        clks_log(CLKS_LOG_ERROR, "FS", external_path);
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_fs_ramdisk_visit(const struct clks_ramdisk_entry *entry, void *ctx) {
    struct clks_fs_build_stats *stats = (struct clks_fs_build_stats *)ctx;

    if (entry == CLKS_NULL || stats == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (entry->type == CLKS_RAMDISK_ENTRY_DIR) {
        if (clks_fs_ensure_dir_hierarchy(entry->path) == CLKS_FALSE) {
            return CLKS_FALSE;
        }

        stats->dir_count++;
        return CLKS_TRUE;
    }

    if (entry->type == CLKS_RAMDISK_ENTRY_FILE) {
        char parent[CLKS_FS_PATH_MAX];
        i32 parent_index;

        if (clks_fs_split_parent(entry->path, parent, sizeof(parent)) == CLKS_FALSE) {
            return CLKS_FALSE;
        }

        if (clks_fs_ensure_dir_hierarchy(parent) == CLKS_FALSE) {
            return CLKS_FALSE;
        }

        parent_index = clks_fs_find_node_by_internal(parent);

        if (parent_index < 0) {
            return CLKS_FALSE;
        }

        if (clks_fs_create_or_update_node(entry->path,
                                          CLKS_FS_NODE_FILE,
                                          (u16)parent_index,
                                          entry->data,
                                          entry->size) < 0) {
            return CLKS_FALSE;
        }

        stats->file_count++;
        return CLKS_TRUE;
    }

    return CLKS_TRUE;
}

void clks_fs_init(void) {
    const struct limine_file *module;
    struct clks_fs_build_stats stats;
    u64 module_count;

    clks_fs_ready = CLKS_FALSE;
    clks_fs_nodes_used = 0U;
    clks_memset(clks_fs_nodes, 0, sizeof(clks_fs_nodes));
    clks_memset(&stats, 0, sizeof(stats));

    if (clks_fs_ensure_root() == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "FS", "FAILED TO CREATE ROOT NODE");
        return;
    }

    module_count = clks_boot_get_module_count();

    if (module_count == 0ULL) {
        clks_log(CLKS_LOG_ERROR, "FS", "NO RAMDISK MODULE FROM LIMINE");
        return;
    }

    module = clks_boot_get_module(0ULL);

    if (module == CLKS_NULL || module->address == CLKS_NULL || module->size == 0ULL) {
        clks_log(CLKS_LOG_ERROR, "FS", "INVALID RAMDISK MODULE");
        return;
    }

    if (clks_ramdisk_iterate(module->address, module->size, clks_fs_ramdisk_visit, &stats) == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "FS", "RAMDISK TAR PARSE FAILED");
        return;
    }

    clks_log(CLKS_LOG_INFO, "FS", "RAMDISK VFS ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "FS", "MODULE_SIZE", module->size);
    clks_log_hex(CLKS_LOG_INFO, "FS", "NODE_COUNT", (u64)clks_fs_nodes_used);
    clks_log_hex(CLKS_LOG_INFO, "FS", "FILE_COUNT", stats.file_count);

    if (clks_fs_require_directory("/system") == CLKS_FALSE) {
        return;
    }

    if (clks_fs_require_directory("/shell") == CLKS_FALSE) {
        return;
    }

    if (clks_fs_require_directory("/temp") == CLKS_FALSE) {
        return;
    }

    if (clks_fs_require_directory("/driver") == CLKS_FALSE) {
        return;
    }

    clks_fs_ready = CLKS_TRUE;
    clks_log(CLKS_LOG_INFO, "FS", "LAYOUT /SYSTEM /SHELL /TEMP /DRIVER OK");
}

clks_bool clks_fs_is_ready(void) {
    return clks_fs_ready;
}

clks_bool clks_fs_stat(const char *path, struct clks_fs_node_info *out_info) {
    i32 node_index;

    if (clks_fs_ready == CLKS_FALSE || out_info == CLKS_NULL) {
        return CLKS_FALSE;
    }

    node_index = clks_fs_find_node_by_external(path);

    if (node_index < 0) {
        return CLKS_FALSE;
    }

    out_info->type = clks_fs_nodes[(u16)node_index].type;
    out_info->size = clks_fs_nodes[(u16)node_index].size;
    return CLKS_TRUE;
}

const void *clks_fs_read_all(const char *path, u64 *out_size) {
    i32 node_index;

    if (clks_fs_ready == CLKS_FALSE) {
        return CLKS_NULL;
    }

    node_index = clks_fs_find_node_by_external(path);

    if (node_index < 0) {
        return CLKS_NULL;
    }

    if (clks_fs_nodes[(u16)node_index].type != CLKS_FS_NODE_FILE) {
        return CLKS_NULL;
    }

    if (out_size != CLKS_NULL) {
        *out_size = clks_fs_nodes[(u16)node_index].size;
    }

    return clks_fs_nodes[(u16)node_index].data;
}

u64 clks_fs_count_children(const char *dir_path) {
    i32 dir_index;
    u64 count = 0ULL;
    u16 i;

    if (clks_fs_ready == CLKS_FALSE) {
        return 0ULL;
    }

    dir_index = clks_fs_find_node_by_external(dir_path);

    if (dir_index < 0) {
        return 0ULL;
    }

    if (clks_fs_nodes[(u16)dir_index].type != CLKS_FS_NODE_DIR) {
        return 0ULL;
    }

    for (i = 0U; i < clks_fs_nodes_used; i++) {
        if (clks_fs_nodes[i].used == CLKS_FALSE) {
            continue;
        }

        if ((u16)dir_index == i) {
            continue;
        }

        if (clks_fs_nodes[i].parent == (u16)dir_index) {
            count++;
        }
    }

    return count;
}

clks_bool clks_fs_get_child_name(const char *dir_path, u64 index, char *out_name, usize out_name_size) {
    i32 dir_index;
    u64 current = 0ULL;
    u16 i;

    if (clks_fs_ready == CLKS_FALSE || out_name == CLKS_NULL || out_name_size == 0U) {
        return CLKS_FALSE;
    }

    dir_index = clks_fs_find_node_by_external(dir_path);

    if (dir_index < 0 || clks_fs_nodes[(u16)dir_index].type != CLKS_FS_NODE_DIR) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < clks_fs_nodes_used; i++) {
        const char *base;
        usize base_len;

        if (clks_fs_nodes[i].used == CLKS_FALSE) {
            continue;
        }

        if ((u16)dir_index == i) {
            continue;
        }

        if (clks_fs_nodes[i].parent != (u16)dir_index) {
            continue;
        }

        if (current != index) {
            current++;
            continue;
        }

        base = clks_fs_basename(clks_fs_nodes[i].path);
        base_len = clks_strlen(base);

        if (base_len + 1U > out_name_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_name, base, base_len + 1U);
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

u64 clks_fs_node_count(void) {
    if (clks_fs_ready == CLKS_FALSE) {
        return 0ULL;
    }

    return (u64)clks_fs_nodes_used;
}
