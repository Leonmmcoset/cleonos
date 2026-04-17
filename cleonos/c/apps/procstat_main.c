#include "cmd_runtime.h"

static const char *ush_procstat_state_name(u64 state) {
    if (state == CLEONOS_PROC_STATE_PENDING) {
        return "PEND";
    }
    if (state == CLEONOS_PROC_STATE_RUNNING) {
        return "RUN ";
    }
    if (state == CLEONOS_PROC_STATE_STOPPED) {
        return "STOP";
    }
    if (state == CLEONOS_PROC_STATE_EXITED) {
        return "EXIT";
    }
    return "UNKN";
}

static int ush_procstat_next_token(const char **io_cursor, char *out, u64 out_size) {
    const char *p;
    u64 n = 0ULL;

    if (io_cursor == (const char **)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out[0] = '\0';
    p = *io_cursor;

    if (p == (const char *)0) {
        return 0;
    }

    while (*p != '\0' && ush_is_space(*p) != 0) {
        p++;
    }

    if (*p == '\0') {
        *io_cursor = p;
        return 0;
    }

    while (*p != '\0' && ush_is_space(*p) == 0) {
        if (n + 1ULL < out_size) {
            out[n++] = *p;
        }
        p++;
    }

    out[n] = '\0';
    *io_cursor = p;
    return 1;
}

static void ush_procstat_print_line(const cleonos_proc_snapshot *snap) {
    if (snap == (const cleonos_proc_snapshot *)0) {
        return;
    }

    ush_write("PID=");
    ush_write_hex_u64(snap->pid);
    ush_write(" ST=");
    ush_write(ush_procstat_state_name(snap->state));
    ush_write(" TTY=");
    ush_write_hex_u64(snap->tty_index);
    ush_write(" RT=");
    ush_write_hex_u64(snap->runtime_ticks);
    ush_write(" MEM=");
    ush_write_hex_u64(snap->mem_bytes);
    ush_write(" SIG=");
    ush_write_hex_u64(snap->last_signal);
    ush_write(" PATH=");
    ush_writeln(snap->path);
}

static void ush_procstat_print_detail(const cleonos_proc_snapshot *snap) {
    if (snap == (const cleonos_proc_snapshot *)0) {
        return;
    }

    ush_writeln("procstat:");
    ush_print_kv_hex("  PID", snap->pid);
    ush_print_kv_hex("  PPID", snap->ppid);
    ush_write("  STATE: ");
    ush_write(ush_procstat_state_name(snap->state));
    ush_write_char('\n');
    ush_print_kv_hex("  STATE_ID", snap->state);
    ush_print_kv_hex("  TTY", snap->tty_index);
    ush_print_kv_hex("  STARTED_TICK", snap->started_tick);
    ush_print_kv_hex("  EXITED_TICK", snap->exited_tick);
    ush_print_kv_hex("  RUNTIME_TICKS", snap->runtime_ticks);
    ush_print_kv_hex("  MEM_BYTES", snap->mem_bytes);
    ush_print_kv_hex("  EXIT_STATUS", snap->exit_status);
    ush_print_kv_hex("  LAST_SIGNAL", snap->last_signal);
    ush_print_kv_hex("  LAST_FAULT_VECTOR", snap->last_fault_vector);
    ush_print_kv_hex("  LAST_FAULT_ERROR", snap->last_fault_error);
    ush_print_kv_hex("  LAST_FAULT_RIP", snap->last_fault_rip);
    ush_write("  PATH: ");
    ush_writeln(snap->path);
}

static int ush_procstat_parse_args(const char *arg, u64 *out_pid, int *out_has_pid, int *out_include_exited) {
    const char *cursor = arg;
    char token[USH_PATH_MAX];
    u64 parsed_pid = 0ULL;
    int has_pid = 0;
    int include_exited = 0;

    if (out_pid == (u64 *)0 || out_has_pid == (int *)0 || out_include_exited == (int *)0) {
        return 0;
    }

    while (ush_procstat_next_token(&cursor, token, (u64)sizeof(token)) != 0) {
        if (ush_streq(token, "-a") != 0 || ush_streq(token, "--all") != 0) {
            include_exited = 1;
            continue;
        }

        if (ush_streq(token, "self") != 0) {
            if (has_pid != 0) {
                return 0;
            }
            parsed_pid = cleonos_sys_getpid();
            has_pid = 1;
            continue;
        }

        if (ush_parse_u64_dec(token, &parsed_pid) != 0 && parsed_pid != 0ULL) {
            if (has_pid != 0) {
                return 0;
            }
            has_pid = 1;
            continue;
        }

        return 0;
    }

    *out_pid = parsed_pid;
    *out_has_pid = has_pid;
    *out_include_exited = include_exited;
    return 1;
}

static int ush_cmd_procstat(const char *arg) {
    u64 target_pid = 0ULL;
    int has_pid = 0;
    int include_exited = 0;

    if (ush_procstat_parse_args(arg, &target_pid, &has_pid, &include_exited) == 0) {
        ush_writeln("procstat: usage procstat [pid|self] [-a|--all]");
        return 0;
    }

    if (has_pid != 0) {
        cleonos_proc_snapshot snap;

        if (cleonos_sys_proc_snapshot(target_pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            ush_writeln("procstat: pid not found");
            return 0;
        }

        ush_procstat_print_detail(&snap);
        return 1;
    }

    {
        u64 proc_count = cleonos_sys_proc_count();
        u64 i;
        u64 shown = 0ULL;

        ush_writeln("procstat:");

        for (i = 0ULL; i < proc_count; i++) {
            u64 pid = 0ULL;
            cleonos_proc_snapshot snap;

            if (cleonos_sys_proc_pid_at(i, &pid) == 0ULL || pid == 0ULL) {
                continue;
            }

            if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
                continue;
            }

            if (include_exited == 0 && snap.state == CLEONOS_PROC_STATE_EXITED) {
                continue;
            }

            ush_procstat_print_line(&snap);
            shown++;
        }

        if (shown == 0ULL) {
            ush_writeln("(no process)");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "procstat") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_procstat(arg);

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
