#include "cmd_runtime.h"
static int ush_cmd_ansi(void) {
    ush_writeln("\x1B[1;36mansi color demo\x1B[0m");
    ush_writeln("  \x1B[30mblack\x1B[0m \x1B[31mred\x1B[0m \x1B[32mgreen\x1B[0m \x1B[33myellow\x1B[0m");
    ush_writeln("  \x1B[34mblue\x1B[0m \x1B[35mmagenta\x1B[0m \x1B[36mcyan\x1B[0m \x1B[37mwhite\x1B[0m");
    ush_writeln("  \x1B[90mbright-black\x1B[0m \x1B[91mbright-red\x1B[0m \x1B[92mbright-green\x1B[0m \x1B[93mbright-yellow\x1B[0m");
    ush_writeln("  \x1B[94mbright-blue\x1B[0m \x1B[95mbright-magenta\x1B[0m \x1B[96mbright-cyan\x1B[0m \x1B[97mbright-white\x1B[0m");
    return 1;
}


int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "ansi") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_ansi();

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

