#include "cmd_runtime.h"
static int ush_copy_file(const char *src_path, const char *dst_path) {
    static char copy_buf[4096];
    u64 src_fd;
    u64 dst_fd;

    if (cleonos_sys_fs_stat_type(src_path) != 1ULL) {
        ush_writeln("cp: source file not found");
        return 0;
    }

    src_fd = cleonos_sys_fd_open(src_path, CLEONOS_O_RDONLY, 0ULL);
    if (src_fd == (u64)-1) {
        ush_writeln("cp: failed to open source");
        return 0;
    }

    dst_fd = cleonos_sys_fd_open(dst_path, CLEONOS_O_WRONLY | CLEONOS_O_CREAT | CLEONOS_O_TRUNC, 0ULL);
    if (dst_fd == (u64)-1) {
        (void)cleonos_sys_fd_close(src_fd);
        ush_writeln("cp: failed to open destination");
        return 0;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read(src_fd, copy_buf, (u64)sizeof(copy_buf));

        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(dst_fd);
            (void)cleonos_sys_fd_close(src_fd);
            ush_writeln("cp: read failed");
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        {
            u64 written_total = 0ULL;
            while (written_total < got) {
                u64 written = cleonos_sys_fd_write(dst_fd,
                                                   copy_buf + written_total,
                                                   got - written_total);
                if (written == (u64)-1 || written == 0ULL) {
                    (void)cleonos_sys_fd_close(dst_fd);
                    (void)cleonos_sys_fd_close(src_fd);
                    ush_writeln("cp: write failed");
                    return 0;
                }
                written_total += written;
            }
        }
    }

    (void)cleonos_sys_fd_close(dst_fd);
    (void)cleonos_sys_fd_close(src_fd);
    return 1;
}

static int ush_cmd_cp(const ush_state *sh, const char *arg) {
    char src_arg[USH_PATH_MAX];
    char dst_arg[USH_PATH_MAX];
    char src_path[USH_PATH_MAX];
    char dst_path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("cp: usage cp <src> <dst>");
        return 0;
    }

    if (ush_split_two_args(arg, src_arg, (u64)sizeof(src_arg), dst_arg, (u64)sizeof(dst_arg)) == 0) {
        ush_writeln("cp: usage cp <src> <dst>");
        return 0;
    }

    if (ush_resolve_path(sh, src_arg, src_path, (u64)sizeof(src_path)) == 0 ||
        ush_resolve_path(sh, dst_arg, dst_path, (u64)sizeof(dst_path)) == 0) {
        ush_writeln("cp: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(dst_path) == 0) {
        ush_writeln("cp: destination must be under /temp");
        return 0;
    }

    return ush_copy_file(src_path, dst_path);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "cp") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_cp(&sh, arg);

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

