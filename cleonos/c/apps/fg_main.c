#include "cmd_runtime.h"

static int ush_fg_is_user_path(const char *path) {
    if (path == (const char *)0 || path[0] != '/') {
        return 0;
    }

    if (path[1] == 's' && path[2] == 'y' && path[3] == 's' && path[4] == 't' && path[5] == 'e' && path[6] == 'm' &&
        (path[7] == '/' || path[7] == '\0')) {
        return 0;
    }

    if (path[1] == 'd' && path[2] == 'r' && path[3] == 'i' && path[4] == 'v' && path[5] == 'e' && path[6] == 'r' &&
        (path[7] == '/' || path[7] == '\0')) {
        return 0;
    }

    return 1;
}

static int ush_fg_pick_latest_job(u64 *out_pid) {
    u64 proc_count;
    u64 tty_active;
    u64 i;
    u64 best = 0ULL;

    if (out_pid == (u64 *)0) {
        return 0;
    }

    *out_pid = 0ULL;
    proc_count = cleonos_sys_proc_count();
    tty_active = cleonos_sys_tty_active();

    for (i = 0ULL; i < proc_count; i++) {
        u64 pid = 0ULL;
        cleonos_proc_snapshot snap;

        if (cleonos_sys_proc_pid_at(i, &pid) == 0ULL || pid == 0ULL) {
            continue;
        }

        if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            continue;
        }

        if (snap.tty_index != tty_active) {
            continue;
        }

        if (ush_fg_is_user_path(snap.path) == 0) {
            continue;
        }

        if (snap.state != CLEONOS_PROC_STATE_PENDING && snap.state != CLEONOS_PROC_STATE_RUNNING) {
            continue;
        }

        if (best == 0ULL || snap.pid > best) {
            best = snap.pid;
        }
    }

    if (best == 0ULL) {
        return 0;
    }

    *out_pid = best;
    return 1;
}

static int ush_fg_wait_pid(u64 pid) {
    for (;;) {
        u64 status = (u64)-1;
        u64 wait_ret = cleonos_sys_wait_pid(pid, &status);

        if (wait_ret == (u64)-1) {
            ush_writeln("fg: pid not found");
            return 0;
        }

        if (wait_ret == 1ULL) {
            ush_write("fg: done [");
            ush_write_hex_u64(pid);
            ush_writeln("]");
            if ((status & (1ULL << 63)) != 0ULL) {
                ush_print_kv_hex("  SIGNAL", status & 0xFFULL);
                ush_print_kv_hex("  VECTOR", (status >> 8) & 0xFFULL);
                ush_print_kv_hex("  ERROR", (status >> 16) & 0xFFFFULL);
            } else {
                ush_print_kv_hex("  STATUS", status);
            }
            return 1;
        }

        (void)cleonos_sys_sleep_ticks(1ULL);
    }
}

static int ush_cmd_fg(const char *arg) {
    u64 pid = 0ULL;

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_parse_u64_dec(arg, &pid) == 0 || pid == 0ULL) {
            ush_writeln("fg: usage fg [pid]");
            return 0;
        }
    } else {
        if (ush_fg_pick_latest_job(&pid) == 0) {
            ush_writeln("fg: no active background job");
            return 0;
        }
    }

    ush_write("fg: waiting [");
    ush_write_hex_u64(pid);
    ush_writeln("]");
    return ush_fg_wait_pid(pid);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "fg") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_fg(arg);

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
