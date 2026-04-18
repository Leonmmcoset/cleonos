#include "cmd_runtime.h"
static int ush_cmd_exec(const ush_state *sh, const char *arg) {
    char target[USH_PATH_MAX];
    char argv_line[USH_ARG_MAX];
    char env_line[USH_PATH_MAX + 32ULL];
    const char *rest = "";
    char path[USH_PATH_MAX];
    u64 status;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("exec: usage exec <path|name> [args...]");
        return 0;
    }

    if (ush_split_first_and_rest(arg, target, (u64)sizeof(target), &rest) == 0) {
        ush_writeln("exec: usage exec <path|name> [args...]");
        return 0;
    }

    argv_line[0] = '\0';
    if (rest != (const char *)0 && rest[0] != '\0') {
        ush_copy(argv_line, (u64)sizeof(argv_line), rest);
    }

    if (ush_resolve_exec_path(sh, target, path, (u64)sizeof(path)) == 0) {
        ush_writeln("exec: invalid target");
        return 0;
    }

    if (ush_path_is_under_system(path) != 0) {
        ush_writeln("exec: /system/*.elf is kernel-mode (KELF), not user-exec");
        return 0;
    }

    env_line[0] = '\0';
    ush_copy(env_line, (u64)sizeof(env_line), "PWD=");
    ush_copy(env_line + 4, (u64)(sizeof(env_line) - 4ULL), sh->cwd);

    status = cleonos_sys_exec_pathv(path, argv_line, env_line);

    if (status == (u64)-1) {
        ush_writeln("exec: request failed");
        return 0;
    }

    if (status == 0ULL) {
        ush_writeln("exec: request accepted");
        return 1;
    }

    if ((status & (1ULL << 63)) != 0ULL) {
        ush_writeln("exec: terminated by signal");
        ush_print_kv_hex("  SIGNAL", status & 0xFFULL);
        ush_print_kv_hex("  VECTOR", (status >> 8) & 0xFFULL);
        ush_print_kv_hex("  ERROR", (status >> 16) & 0xFFFFULL);
    } else {
        ush_writeln("exec: returned non-zero status");
        ush_print_kv_hex("  STATUS", status);
    }

    return 0;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "exec") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_exec(&sh, arg);

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
