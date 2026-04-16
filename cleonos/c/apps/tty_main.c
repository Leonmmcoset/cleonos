#include "cmd_runtime.h"
static int ush_cmd_tty(const char *arg) {
    u64 tty_count = cleonos_sys_tty_count();
    u64 active = cleonos_sys_tty_active();

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_print_kv_hex("TTY_COUNT", tty_count);
        ush_print_kv_hex("TTY_ACTIVE", active);
        return 1;
    }

    {
        u64 idx;

        if (ush_parse_u64_dec(arg, &idx) == 0) {
            ush_writeln("tty: usage tty [index]");
            return 0;
        }

        if (idx >= tty_count) {
            ush_writeln("tty: index out of range");
            return 0;
        }

        if (cleonos_sys_tty_switch(idx) == (u64)-1) {
            ush_writeln("tty: switch failed");
            return 0;
        }

        ush_writeln("tty: switched");
        ush_print_kv_hex("TTY_ACTIVE", cleonos_sys_tty_active());
        return 1;
    }
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "tty") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_tty(arg);

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

