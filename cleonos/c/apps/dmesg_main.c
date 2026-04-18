#include "cmd_runtime.h"
static int ush_cmd_dmesg(const char *arg) {
    u64 total = cleonos_sys_log_journal_count();
    u64 limit = USH_DMESG_DEFAULT;
    u64 start;
    u64 i;

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_parse_u64_dec(arg, &limit) == 0 || limit == 0ULL) {
            ush_writeln("dmesg: usage dmesg [positive_count]");
            return 0;
        }
    }

    if (total == 0ULL) {
        ush_writeln("(journal empty)");
        return 1;
    }

    if (limit > total) {
        limit = total;
    }

    start = total - limit;

    for (i = start; i < total; i++) {
        char line[USH_DMESG_LINE_MAX];

        if (cleonos_sys_log_journal_read(i, line, (u64)sizeof(line)) != 0ULL) {
            ush_writeln(line);
        }
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "dmesg") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_dmesg(arg);

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
