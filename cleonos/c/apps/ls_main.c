#include "cmd_runtime.h"
static int ush_ls_join_path(const char *dir_path, const char *name, char *out_path, u64 out_size) {
    u64 p = 0ULL;
    u64 i;

    if (dir_path == (const char *)0 || name == (const char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (dir_path[0] == '/' && dir_path[1] == '\0') {
        if (out_size < 2ULL) {
            return 0;
        }

        out_path[p++] = '/';
    } else {
        for (i = 0ULL; dir_path[i] != '\0'; i++) {
            if (p + 1ULL >= out_size) {
                return 0;
            }

            out_path[p++] = dir_path[i];
        }

        if (p == 0ULL || out_path[p - 1ULL] != '/') {
            if (p + 1ULL >= out_size) {
                return 0;
            }

            out_path[p++] = '/';
        }
    }

    for (i = 0ULL; name[i] != '\0'; i++) {
        if (p + 1ULL >= out_size) {
            return 0;
        }

        out_path[p++] = name[i];
    }

    out_path[p] = '\0';
    return 1;
}

static const char *ush_ls_basename(const char *path) {
    const char *name = path;
    u64 i = 0ULL;

    if (path == (const char *)0 || path[0] == '\0') {
        return "";
    }

    while (path[i] != '\0') {
        if (path[i] == '/' && path[i + 1ULL] != '\0') {
            name = &path[i + 1ULL];
        }

        i++;
    }

    return name;
}

static int ush_ls_is_dot_entry(const char *name) {
    if (name == (const char *)0) {
        return 0;
    }

    if (name[0] == '.' && name[1] == '\0') {
        return 1;
    }

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return 1;
    }

    return 0;
}

static void ush_ls_print_one(const char *name, u64 type, u64 size, int long_mode) {
    if (long_mode == 0) {
        ush_writeln(name);
        return;
    }

    if (type == 2ULL) {
        ush_write("d ");
    } else if (type == 1ULL) {
        ush_write("f ");
    } else {
        ush_write("? ");
    }

    ush_write(name);

    if (type == 1ULL) {
        ush_write("  size=");
        ush_write_hex_u64(size);
    } else if (type == 2ULL) {
        ush_write("  <DIR>");
    } else {
        ush_write("  <UNKNOWN>");
    }

    ush_write_char('\n');
}

static int ush_ls_parse_args(const char *arg,
                             int *out_long_mode,
                             int *out_recursive,
                             char *out_target,
                             u64 out_target_size) {
    char token[USH_PATH_MAX];
    u64 i = 0ULL;
    int path_set = 0;

    if (out_long_mode == (int *)0 ||
        out_recursive == (int *)0 ||
        out_target == (char *)0 ||
        out_target_size == 0ULL) {
        return 0;
    }

    *out_long_mode = 0;
    *out_recursive = 0;
    ush_copy(out_target, out_target_size, ".");

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    while (arg[i] != '\0') {
        u64 p = 0ULL;
        u64 j;

        while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
            i++;
        }

        if (arg[i] == '\0') {
            break;
        }

        while (arg[i] != '\0' && ush_is_space(arg[i]) == 0) {
            if (p + 1ULL < (u64)sizeof(token)) {
                token[p++] = arg[i];
            }

            i++;
        }

        token[p] = '\0';

        if (token[0] == '-' && token[1] != '\0') {
            for (j = 1ULL; token[j] != '\0'; j++) {
                if (token[j] == 'l') {
                    *out_long_mode = 1;
                } else if (token[j] == 'R') {
                    *out_recursive = 1;
                } else {
                    return 0;
                }
            }

            continue;
        }

        if (path_set != 0) {
            return 0;
        }

        ush_copy(out_target, out_target_size, token);
        path_set = 1;
    }

    return 1;
}

static int ush_ls_dir(const char *path,
                      int long_mode,
                      int recursive,
                      int print_header,
                      u64 depth) {
    u64 count;
    u64 i;

    if (depth > 16ULL) {
        ush_writeln("ls: recursion depth limit reached");
        return 0;
    }

    count = cleonos_sys_fs_child_count(path);

    if (print_header != 0) {
        ush_write(path);
        ush_writeln(":");
    }

    if (count == 0ULL) {
        ush_writeln("(empty)");
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_path[USH_PATH_MAX];
        u64 type;
        u64 size = 0ULL;

        name[0] = '\0';

        if (cleonos_sys_fs_get_child_name(path, i, name) == 0ULL) {
            continue;
        }

        if (ush_ls_join_path(path, name, child_path, (u64)sizeof(child_path)) == 0) {
            continue;
        }

        type = cleonos_sys_fs_stat_type(child_path);

        if (type == 1ULL) {
            size = cleonos_sys_fs_stat_size(child_path);
        }

        ush_ls_print_one(name, type, size, long_mode);
    }

    if (recursive == 0) {
        return 1;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_path[USH_PATH_MAX];

        name[0] = '\0';

        if (cleonos_sys_fs_get_child_name(path, i, name) == 0ULL) {
            continue;
        }

        if (ush_ls_is_dot_entry(name) != 0) {
            continue;
        }

        if (ush_ls_join_path(path, name, child_path, (u64)sizeof(child_path)) == 0) {
            continue;
        }

        if (cleonos_sys_fs_stat_type(child_path) == 2ULL) {
            ush_write_char('\n');
            (void)ush_ls_dir(child_path, long_mode, recursive, 1, depth + 1ULL);
        }
    }

    return 1;
}

static int ush_cmd_ls(const ush_state *sh, const char *arg) {
    char target[USH_PATH_MAX];
    char path[USH_PATH_MAX];
    u64 type;
    int long_mode;
    int recursive;

    if (ush_ls_parse_args(arg, &long_mode, &recursive, target, (u64)sizeof(target)) == 0) {
        ush_writeln("ls: usage ls [-l] [-R] [path]");
        return 0;
    }

    if (ush_resolve_path(sh, target, path, (u64)sizeof(path)) == 0) {
        ush_writeln("ls: invalid path");
        return 0;
    }

    type = cleonos_sys_fs_stat_type(path);

    if (type == 1ULL) {
        u64 size = cleonos_sys_fs_stat_size(path);
        ush_ls_print_one(ush_ls_basename(path), type, size, long_mode);
        return 1;
    }

    if (type != 2ULL) {
        ush_writeln("ls: path not found");
        return 0;
    }

    return ush_ls_dir(path, long_mode, recursive, recursive, 0ULL);
}


int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "ls") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_ls(&sh, arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}

