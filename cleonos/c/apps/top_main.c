#include "cmd_runtime.h"

static const char *ush_top_state_name(u64 state) {
    if (state == CLEONOS_PROC_STATE_PENDING) {
        return "PEND";
    }
    if (state == CLEONOS_PROC_STATE_RUNNING) {
        return "RUN ";
    }
    if (state == CLEONOS_PROC_STATE_EXITED) {
        return "EXIT";
    }
    return "UNKN";
}

static int ush_top_next_token(const char **io_cursor, char *out, u64 out_size) {
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

static int ush_top_parse(const char *arg, u64 *out_loops, u64 *out_delay) {
    const char *cursor = arg;
    char token[USH_PATH_MAX];
    u64 loops = 0ULL;
    u64 delay = 5ULL;

    if (out_loops == (u64 *)0 || out_delay == (u64 *)0) {
        return 0;
    }

    while (ush_top_next_token(&cursor, token, (u64)sizeof(token)) != 0) {
        if (ush_streq(token, "--once") != 0) {
            loops = 1ULL;
            continue;
        }

        if (ush_streq(token, "-n") != 0) {
            if (ush_top_next_token(&cursor, token, (u64)sizeof(token)) == 0 || ush_parse_u64_dec(token, &loops) == 0) {
                return 0;
            }
            continue;
        }

        if (ush_streq(token, "-d") != 0) {
            if (ush_top_next_token(&cursor, token, (u64)sizeof(token)) == 0 || ush_parse_u64_dec(token, &delay) == 0) {
                return 0;
            }
            continue;
        }

        return 0;
    }

    *out_loops = loops;
    *out_delay = delay;
    return 1;
}

static void ush_top_render_frame(u64 frame_index, u64 delay_ticks) {
    u64 proc_count = cleonos_sys_proc_count();
    u64 i;
    u64 shown = 0ULL;

    ush_write("\x1B[2J\x1B[H");
    ush_write("top frame=");
    ush_write_hex_u64(frame_index);
    ush_write(" ticks=");
    ush_write_hex_u64(cleonos_sys_timer_ticks());
    ush_write(" delay=");
    ush_write_hex_u64(delay_ticks);
    ush_write_char('\n');
    ush_writeln("PID      ST   TTY    RTICKS           MEM              PATH");

    for (i = 0ULL; i < proc_count; i++) {
        u64 pid = 0ULL;
        cleonos_proc_snapshot snap;

        if (cleonos_sys_proc_pid_at(i, &pid) == 0ULL || pid == 0ULL) {
            continue;
        }

        if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            continue;
        }

        if (snap.state != CLEONOS_PROC_STATE_PENDING && snap.state != CLEONOS_PROC_STATE_RUNNING) {
            continue;
        }

        ush_write_hex_u64(snap.pid);
        ush_write(" ");
        ush_write(ush_top_state_name(snap.state));
        ush_write(" ");
        ush_write_hex_u64(snap.tty_index);
        ush_write(" ");
        ush_write_hex_u64(snap.runtime_ticks);
        ush_write(" ");
        ush_write_hex_u64(snap.mem_bytes);
        ush_write(" ");
        ush_writeln(snap.path);
        shown++;
    }

    if (shown == 0ULL) {
        ush_writeln("(no active process)");
    }

    ush_writeln("");
    ush_writeln("press q to quit");
}

static int ush_top_sleep_or_quit(u64 delay_ticks) {
    u64 i;

    if (delay_ticks == 0ULL) {
        delay_ticks = 1ULL;
    }

    for (i = 0ULL; i < delay_ticks; i++) {
        u64 ch = cleonos_sys_kbd_get_char();

        if (ch != (u64)-1) {
            char c = (char)(ch & 0xFFULL);

            if (c == 'q' || c == 'Q') {
                return 1;
            }
        }

        (void)cleonos_sys_sleep_ticks(1ULL);
    }

    return 0;
}

static int ush_cmd_top(const char *arg) {
    u64 loops;
    u64 delay_ticks;
    u64 frame = 0ULL;

    if (ush_top_parse(arg, &loops, &delay_ticks) == 0) {
        ush_writeln("top: usage top [--once] [-n loops] [-d ticks]");
        return 0;
    }

    for (;;) {
        frame++;
        ush_top_render_frame(frame, delay_ticks);

        if (loops != 0ULL && frame >= loops) {
            break;
        }

        if (ush_top_sleep_or_quit(delay_ticks) != 0) {
            break;
        }
    }

    ush_write("\x1B[0m");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "top") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_top(arg);

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
