#include "cmd_runtime.h"
static u64 ush_fastfetch_u64_to_dec(char *out, u64 out_size, u64 value) {
    char rev[32];
    u64 digits = 0ULL;
    u64 i;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0ULL;
    }

    if (value == 0ULL) {
        if (out_size < 2ULL) {
            return 0ULL;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1ULL;
    }

    while (value > 0ULL && digits < (u64)sizeof(rev)) {
        rev[digits++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    if (digits + 1ULL > out_size) {
        out[0] = '\0';
        return 0ULL;
    }

    for (i = 0ULL; i < digits; i++) {
        out[i] = rev[digits - 1ULL - i];
    }
    out[digits] = '\0';
    return digits;
}

static void ush_fastfetch_write_u64_dec(u64 value) {
    char text[32];

    if (ush_fastfetch_u64_to_dec(text, (u64)sizeof(text), value) == 0ULL) {
        ush_write("0");
        return;
    }

    ush_write(text);
}

static void ush_fastfetch_write_key(int plain, const char *key) {
    ush_write("  ");
    if (plain == 0) {
        ush_write("\x1B[1;96m");
    }
    ush_write(key);
    if (plain == 0) {
        ush_write("\x1B[0m");
    }
    ush_write(": ");
}

static void ush_fastfetch_print_text(int plain, const char *key, const char *value) {
    ush_fastfetch_write_key(plain, key);
    ush_writeln(value);
}

static void ush_fastfetch_print_u64(int plain, const char *key, u64 value) {
    ush_fastfetch_write_key(plain, key);
    ush_fastfetch_write_u64_dec(value);
    ush_write_char('\n');
}

static void ush_fastfetch_print_logo(int plain) {
    if (plain == 0) {
        ush_writeln("\x1B[1;34m $$$$$$\\  $$\\                                      $$$$$$\\   $$$$$$\\  \x1B[0m");
        ush_writeln("\x1B[1;36m$$  __$$\\ $$ |                                    $$  __$$\\ $$  __$$\\ \x1B[0m");
        ush_writeln("\x1B[1;32m$$ /  \\__|$$ |       $$$$$$\\   $$$$$$\\  $$$$$$$\\  $$ /  $$ |$$ /  \\__|\x1B[0m");
        ush_writeln("\x1B[1;33m$$ |      $$ |      $$  __$$\\ $$  __$$\\ $$  __$$\\ $$ |  $$ |\\$$$$$$\\  \x1B[0m");
        ush_writeln("\x1B[1;31m$$ |      $$ |      $$$$$$$$ |$$ /  $$ |$$ |  $$ |$$ |  $$ | \\____$$\\ \x1B[0m");
        ush_writeln("\x1B[1;35m$$ |  $$\\ $$ |      $$   ____|$$ |  $$ |$$ |  $$ |$$ |  $$ |$$\\   $$ |\x1B[0m");
        ush_writeln("\x1B[1;94m\\$$$$$$  |$$$$$$$$\\ \\$$$$$$$\\ \\$$$$$$  |$$ |  $$ | $$$$$$  |\\$$$$$$  |\x1B[0m");
        ush_writeln("\x1B[1;96m \\______/ \\________| \\_______| \\______/ \\__|  \\__| \\______/  \\______/ \x1B[0m");
        ush_writeln("                                                                      ");
        ush_writeln("                                                                      ");
    } else {
        ush_writeln(" $$$$$$\\  $$\\                                      $$$$$$\\   $$$$$$\\  ");
        ush_writeln("$$  __$$\\ $$ |                                    $$  __$$\\ $$  __$$\\ ");
        ush_writeln("$$ /  \\__|$$ |       $$$$$$\\   $$$$$$\\  $$$$$$$\\  $$ /  $$ |$$ /  \\__|");
        ush_writeln("$$ |      $$ |      $$  __$$\\ $$  __$$\\ $$  __$$\\ $$ |  $$ |\\$$$$$$\\  ");
        ush_writeln("$$ |      $$ |      $$$$$$$$ |$$ /  $$ |$$ |  $$ |$$ |  $$ | \\____$$\\ ");
        ush_writeln("$$ |  $$\\ $$ |      $$   ____|$$ |  $$ |$$ |  $$ |$$ |  $$ |$$\\   $$ |");
        ush_writeln("\\$$$$$$  |$$$$$$$$\\ \\$$$$$$$\\ \\$$$$$$  |$$ |  $$ | $$$$$$  |\\$$$$$$  |");
        ush_writeln(" \\______/ \\________| \\_______| \\______/ \\__|  \\__| \\______/  \\______/ ");
        ush_writeln("                                                                      ");
        ush_writeln("                                                                      ");
    }
}

static void ush_fastfetch_print_palette(int plain) {
    ush_fastfetch_write_key(plain, "Palette");

    if (plain != 0) {
        ush_writeln("ANSI16");
        return;
    }

    ush_write("\x1B[40m  \x1B[0m\x1B[41m  \x1B[0m\x1B[42m  \x1B[0m\x1B[43m  \x1B[0m");
    ush_write("\x1B[44m  \x1B[0m\x1B[45m  \x1B[0m\x1B[46m  \x1B[0m\x1B[47m  \x1B[0m ");
    ush_write("\x1B[100m  \x1B[0m\x1B[101m  \x1B[0m\x1B[102m  \x1B[0m\x1B[103m  \x1B[0m");
    ush_write("\x1B[104m  \x1B[0m\x1B[105m  \x1B[0m\x1B[106m  \x1B[0m\x1B[107m  \x1B[0m");
    ush_write_char('\n');
}

static int ush_cmd_fastfetch(const char *arg) {
    int plain = 0;
    u64 tty_active;
    u64 tty_count;
    u64 exec_req;
    u64 exec_ok;

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_streq(arg, "--plain") != 0) {
            plain = 1;
        } else if (ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0) {
            ush_writeln("usage: fastfetch [--plain]");
            return 1;
        } else {
            ush_writeln("fastfetch: usage fastfetch [--plain]");
            return 0;
        }
    }

    tty_active = cleonos_sys_tty_active();
    tty_count = cleonos_sys_tty_count();
    exec_req = cleonos_sys_exec_request_count();
    exec_ok = cleonos_sys_exec_success_count();

    ush_fastfetch_print_logo(plain);
    ush_write_char('\n');

    ush_fastfetch_print_text(plain, "OS", "CLeonOS x86_64");
    ush_fastfetch_print_text(plain, "Shell", "User Shell (/shell/shell.elf)");
    ush_fastfetch_print_u64(plain, "PID", cleonos_sys_getpid());
    ush_fastfetch_print_u64(plain, "UptimeTicks", cleonos_sys_timer_ticks());
    ush_fastfetch_print_u64(plain, "Tasks", cleonos_sys_task_count());
    ush_fastfetch_print_u64(plain, "Services", cleonos_sys_service_count());
    ush_fastfetch_print_u64(plain, "SvcReady", cleonos_sys_service_ready_count());
    ush_fastfetch_print_u64(plain, "CtxSwitches", cleonos_sys_context_switches());
    ush_fastfetch_print_u64(plain, "KELFApps", cleonos_sys_kelf_count());
    ush_fastfetch_print_u64(plain, "KELFRuns", cleonos_sys_kelf_runs());
    ush_fastfetch_print_u64(plain, "FSNodes", cleonos_sys_fs_node_count());
    ush_fastfetch_print_u64(plain, "RootChildren", cleonos_sys_fs_child_count("/"));

    ush_fastfetch_write_key(plain, "TTY");
    ush_fastfetch_write_u64_dec(tty_active);
    ush_write(" / ");
    ush_fastfetch_write_u64_dec(tty_count);
    ush_write_char('\n');

    ush_fastfetch_write_key(plain, "ExecSuccess");
    ush_fastfetch_write_u64_dec(exec_ok);
    ush_write(" / ");
    ush_fastfetch_write_u64_dec(exec_req);
    ush_write_char('\n');

    ush_fastfetch_print_u64(plain, "KbdBuffered", cleonos_sys_kbd_buffered());
    ush_fastfetch_print_palette(plain);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "fastfetch") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_fastfetch(arg);

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

