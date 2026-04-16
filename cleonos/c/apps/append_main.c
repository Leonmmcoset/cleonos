#include "cmd_runtime.h"
static int ush_cmd_append(const ush_state *sh, const char *arg) {
    char path_arg[USH_PATH_MAX];
    char abs_path[USH_PATH_MAX];
    const char *payload = (const char *)0;
    u64 payload_len;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("append: usage append <file> <text>");
        return 0;
    }

    if (ush_split_first_and_rest(arg, path_arg, (u64)sizeof(path_arg), &payload) == 0) {
        ush_writeln("append: usage append <file> <text>");
        return 0;
    }

    if (ush_resolve_path(sh, path_arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln("append: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(abs_path) == 0) {
        ush_writeln("append: target must be under /temp");
        return 0;
    }

    if (payload == (const char *)0 || payload[0] == '\0') {
        if (ush_pipeline_stdin_text == (const char *)0) {
            ush_writeln("append: usage append <file> <text>");
            return 0;
        }
        payload = ush_pipeline_stdin_text;
        payload_len = ush_pipeline_stdin_len;
    } else {
        payload_len = ush_strlen(payload);
    }

    if (cleonos_sys_fs_append(abs_path, payload, payload_len) == 0ULL) {
        ush_writeln("append: failed");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "append") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_append(&sh, arg);

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

