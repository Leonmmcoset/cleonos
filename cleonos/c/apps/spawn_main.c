#include "cmd_runtime.h"
static int ush_cmd_spawn(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];
    u64 pid;

    if (ush_resolve_exec_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln("spawn: invalid target");
        return 0;
    }

    if (ush_path_is_under_system(path) != 0) {
        ush_writeln("spawn: /system/*.elf is kernel-mode (KELF), not user-exec");
        return 0;
    }

    pid = cleonos_sys_spawn_path(path);

    if (pid == (u64)-1) {
        ush_writeln("spawn: request failed");
        return 0;
    }

    ush_writeln("spawn: completed");
    ush_print_kv_hex("  PID", pid);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "spawn") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_spawn(&sh, arg);

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

