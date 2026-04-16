#include "cmd_runtime.h"
static int ush_copy_file(const char *src_path, const char *dst_path) {
    static char copy_buf[USH_COPY_MAX];
    u64 src_type;
    u64 src_size;
    u64 got;

    src_type = cleonos_sys_fs_stat_type(src_path);

    if (src_type != 1ULL) {
        ush_writeln("cp: source file not found");
        return 0;
    }

    src_size = cleonos_sys_fs_stat_size(src_path);

    if (src_size == (u64)-1) {
        ush_writeln("cp: failed to stat source");
        return 0;
    }

    if (src_size > (u64)USH_COPY_MAX) {
        ush_writeln("cp: source too large for user shell buffer");
        return 0;
    }

    if (src_size == 0ULL) {
        got = 0ULL;
    } else {
        got = cleonos_sys_fs_read(src_path, copy_buf, src_size);

        if (got == 0ULL || got != src_size) {
            ush_writeln("cp: failed to read source");
            return 0;
        }
    }

    if (cleonos_sys_fs_write(dst_path, copy_buf, got) == 0ULL) {
        ush_writeln("cp: failed to write destination");
        return 0;
    }

    return 1;
}

static int ush_cmd_mv(const ush_state *sh, const char *arg) {
    char src_arg[USH_PATH_MAX];
    char dst_arg[USH_PATH_MAX];
    char src_path[USH_PATH_MAX];
    char dst_path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("mv: usage mv <src> <dst>");
        return 0;
    }

    if (ush_split_two_args(arg, src_arg, (u64)sizeof(src_arg), dst_arg, (u64)sizeof(dst_arg)) == 0) {
        ush_writeln("mv: usage mv <src> <dst>");
        return 0;
    }

    if (ush_resolve_path(sh, src_arg, src_path, (u64)sizeof(src_path)) == 0 ||
        ush_resolve_path(sh, dst_arg, dst_path, (u64)sizeof(dst_path)) == 0) {
        ush_writeln("mv: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(src_path) == 0 || ush_path_is_under_temp(dst_path) == 0) {
        ush_writeln("mv: source and destination must be under /temp");
        return 0;
    }

    if (ush_copy_file(src_path, dst_path) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_remove(src_path) == 0ULL) {
        ush_writeln("mv: source remove failed");
        return 0;
    }

    return 1;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "mv") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_mv(&sh, arg);

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

