#include "cmd_runtime.h"

static int ush_cmd_kill(const char *arg) {
    char pid_text[USH_PATH_MAX];
    const char *rest = "";
    u64 pid;
    u64 signal = 15ULL;
    u64 ret;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("kill: usage kill <pid> [signal]");
        return 0;
    }

    if (ush_split_first_and_rest(arg, pid_text, (u64)sizeof(pid_text), &rest) == 0) {
        ush_writeln("kill: usage kill <pid> [signal]");
        return 0;
    }

    if (ush_parse_u64_dec(pid_text, &pid) == 0 || pid == 0ULL) {
        ush_writeln("kill: invalid pid");
        return 0;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_parse_u64_dec(rest, &signal) == 0 || signal > 255ULL) {
            ush_writeln("kill: invalid signal");
            return 0;
        }
    }

    ret = cleonos_sys_proc_kill(pid, signal);

    if (ret == (u64)-1) {
        ush_writeln("kill: pid not found");
        return 0;
    }

    if (ret == 0ULL) {
        ush_writeln("kill: target cannot be terminated right now");
        return 0;
    }

    ush_write("kill: sent signal ");
    ush_write_hex_u64(signal);
    ush_write(" to ");
    ush_write_hex_u64(pid);
    ush_write_char('\n');
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "kill") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_kill(arg);

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
