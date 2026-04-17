#include "cmd_runtime.h"

static int ush_jobs_is_user_path(const char *path) {
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

static const char *ush_jobs_state_name(u64 state) {
    if (state == CLEONOS_PROC_STATE_PENDING) {
        return "PENDING";
    }
    if (state == CLEONOS_PROC_STATE_RUNNING) {
        return "RUNNING";
    }
    if (state == CLEONOS_PROC_STATE_STOPPED) {
        return "STOPPED";
    }
    if (state == CLEONOS_PROC_STATE_EXITED) {
        return "EXITED ";
    }
    return "UNUSED ";
}

static int ush_cmd_jobs(const char *arg) {
    u64 proc_count;
    u64 tty_active = cleonos_sys_tty_active();
    u64 i;
    u64 shown = 0ULL;
    int include_exited = 0;

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_streq(arg, "-a") != 0 || ush_streq(arg, "--all") != 0) {
            include_exited = 1;
        } else {
            ush_writeln("jobs: usage jobs [-a|--all]");
            return 0;
        }
    }

    proc_count = cleonos_sys_proc_count();
    ush_writeln("jobs:");

    for (i = 0ULL; i < proc_count; i++) {
        u64 pid = 0ULL;
        cleonos_proc_snapshot snap;
        const char *state_name;

        if (cleonos_sys_proc_pid_at(i, &pid) == 0ULL || pid == 0ULL) {
            continue;
        }

        if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            continue;
        }

        if (ush_jobs_is_user_path(snap.path) == 0) {
            continue;
        }

        if (snap.tty_index != tty_active) {
            continue;
        }

        if (include_exited == 0 && snap.state == CLEONOS_PROC_STATE_EXITED) {
            continue;
        }

        state_name = ush_jobs_state_name(snap.state);
        ush_write("[");
        ush_write_hex_u64(snap.pid);
        ush_write("] ");
        ush_write(state_name);
        ush_write("  ");
        ush_write(snap.path);
        if (snap.state == CLEONOS_PROC_STATE_EXITED) {
            ush_write("  status=");
            ush_write_hex_u64(snap.exit_status);
        }
        ush_write_char('\n');
        shown++;
    }

    if (shown == 0ULL) {
        ush_writeln("(no jobs)");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "jobs") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_jobs(arg);

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
