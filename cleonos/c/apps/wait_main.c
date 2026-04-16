#include "cmd_runtime.h"
static int ush_cmd_wait(const char *arg) {
    u64 pid;
    u64 status = (u64)-1;
    u64 wait_ret;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("wait: usage wait <pid>");
        return 0;
    }

    if (ush_parse_u64_dec(arg, &pid) == 0) {
        ush_writeln("wait: invalid pid");
        return 0;
    }

    wait_ret = cleonos_sys_wait_pid(pid, &status);

    if (wait_ret == (u64)-1) {
        ush_writeln("wait: pid not found");
        return 0;
    }

    if (wait_ret == 0ULL) {
        ush_writeln("wait: still running");
        return 1;
    }

    ush_writeln("wait: exited");
    ush_print_kv_hex("  STATUS", status);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "wait") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_wait(arg);

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

