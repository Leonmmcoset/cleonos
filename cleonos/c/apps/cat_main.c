#include "cmd_runtime.h"
#include <stdio.h>

static int ush_cmd_cat(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];
    char buf[USH_CAT_MAX + 1ULL];
    u64 size;
    u64 req;
    u64 got;

    if (arg == (const char *)0 || arg[0] == '\0') {
        if (ush_pipeline_stdin_text != (const char *)0 && ush_pipeline_stdin_len > 0ULL) {
            (void)fputs(ush_pipeline_stdin_text, 1);
            return 1;
        }

        (void)puts("cat: file path required");
        return 0;
    }

    if (ush_resolve_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        (void)puts("cat: invalid path");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(path) != 1ULL) {
        (void)puts("cat: file not found");
        return 0;
    }

    size = cleonos_sys_fs_stat_size(path);

    if (size == (u64)-1) {
        (void)puts("cat: failed to stat file");
        return 0;
    }

    if (size == 0ULL) {
        return 1;
    }

    req = (size < USH_CAT_MAX) ? size : USH_CAT_MAX;
    got = cleonos_sys_fs_read(path, buf, req);

    if (got == 0ULL) {
        (void)puts("cat: read failed");
        return 0;
    }

    if (got > USH_CAT_MAX) {
        got = USH_CAT_MAX;
    }

    buf[got] = '\0';
    (void)puts(buf);

    if (size > got) {
        (void)puts("[cat] output truncated");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "cat") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_cat(&sh, arg);

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

