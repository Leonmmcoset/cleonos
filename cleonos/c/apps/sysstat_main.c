#include "cmd_runtime.h"

#define USH_SYSSTAT_MAX_IDS (CLEONOS_SYSCALL_STATS_RECENT_ID + 1ULL)
#define USH_SYSSTAT_DEFAULT_TOP 12ULL

typedef struct ush_sysstat_entry {
    u64 id;
    u64 recent;
    u64 total;
    const char *name;
} ush_sysstat_entry;

static const char *ush_sysstat_name_for_id(u64 id) {
    switch (id) {
    case CLEONOS_SYSCALL_LOG_WRITE:
        return "LOG_WRITE";
    case CLEONOS_SYSCALL_TIMER_TICKS:
        return "TIMER_TICKS";
    case CLEONOS_SYSCALL_TASK_COUNT:
        return "TASK_COUNT";
    case CLEONOS_SYSCALL_CUR_TASK:
        return "CUR_TASK";
    case CLEONOS_SYSCALL_SERVICE_COUNT:
        return "SERVICE_COUNT";
    case CLEONOS_SYSCALL_SERVICE_READY_COUNT:
        return "SERVICE_READY";
    case CLEONOS_SYSCALL_CONTEXT_SWITCHES:
        return "CONTEXT_SWITCH";
    case CLEONOS_SYSCALL_KELF_COUNT:
        return "KELF_COUNT";
    case CLEONOS_SYSCALL_KELF_RUNS:
        return "KELF_RUNS";
    case CLEONOS_SYSCALL_FS_NODE_COUNT:
        return "FS_NODE_COUNT";
    case CLEONOS_SYSCALL_FS_CHILD_COUNT:
        return "FS_CHILD_COUNT";
    case CLEONOS_SYSCALL_FS_GET_CHILD_NAME:
        return "FS_CHILD_NAME";
    case CLEONOS_SYSCALL_FS_READ:
        return "FS_READ";
    case CLEONOS_SYSCALL_EXEC_PATH:
        return "EXEC_PATH";
    case CLEONOS_SYSCALL_EXEC_REQUESTS:
        return "EXEC_REQUESTS";
    case CLEONOS_SYSCALL_EXEC_SUCCESS:
        return "EXEC_SUCCESS";
    case CLEONOS_SYSCALL_USER_SHELL_READY:
        return "USER_SHELL_READY";
    case CLEONOS_SYSCALL_USER_EXEC_REQUESTED:
        return "USER_EXEC_REQ";
    case CLEONOS_SYSCALL_USER_LAUNCH_TRIES:
        return "USER_LAUNCH_TRY";
    case CLEONOS_SYSCALL_USER_LAUNCH_OK:
        return "USER_LAUNCH_OK";
    case CLEONOS_SYSCALL_USER_LAUNCH_FAIL:
        return "USER_LAUNCH_FAIL";
    case CLEONOS_SYSCALL_TTY_COUNT:
        return "TTY_COUNT";
    case CLEONOS_SYSCALL_TTY_ACTIVE:
        return "TTY_ACTIVE";
    case CLEONOS_SYSCALL_TTY_SWITCH:
        return "TTY_SWITCH";
    case CLEONOS_SYSCALL_TTY_WRITE:
        return "TTY_WRITE";
    case CLEONOS_SYSCALL_TTY_WRITE_CHAR:
        return "TTY_WRITE_CHAR";
    case CLEONOS_SYSCALL_KBD_GET_CHAR:
        return "KBD_GET_CHAR";
    case CLEONOS_SYSCALL_FS_STAT_TYPE:
        return "FS_STAT_TYPE";
    case CLEONOS_SYSCALL_FS_STAT_SIZE:
        return "FS_STAT_SIZE";
    case CLEONOS_SYSCALL_FS_MKDIR:
        return "FS_MKDIR";
    case CLEONOS_SYSCALL_FS_WRITE:
        return "FS_WRITE";
    case CLEONOS_SYSCALL_FS_APPEND:
        return "FS_APPEND";
    case CLEONOS_SYSCALL_FS_REMOVE:
        return "FS_REMOVE";
    case CLEONOS_SYSCALL_LOG_JOURNAL_COUNT:
        return "LOG_JCOUNT";
    case CLEONOS_SYSCALL_LOG_JOURNAL_READ:
        return "LOG_JREAD";
    case CLEONOS_SYSCALL_KBD_BUFFERED:
        return "KBD_BUFFERED";
    case CLEONOS_SYSCALL_KBD_PUSHED:
        return "KBD_PUSHED";
    case CLEONOS_SYSCALL_KBD_POPPED:
        return "KBD_POPPED";
    case CLEONOS_SYSCALL_KBD_DROPPED:
        return "KBD_DROPPED";
    case CLEONOS_SYSCALL_KBD_HOTKEY_SWITCHES:
        return "KBD_HOTKEYS";
    case CLEONOS_SYSCALL_GETPID:
        return "GETPID";
    case CLEONOS_SYSCALL_SPAWN_PATH:
        return "SPAWN_PATH";
    case CLEONOS_SYSCALL_WAITPID:
        return "WAITPID";
    case CLEONOS_SYSCALL_EXIT:
        return "EXIT";
    case CLEONOS_SYSCALL_SLEEP_TICKS:
        return "SLEEP_TICKS";
    case CLEONOS_SYSCALL_YIELD:
        return "YIELD";
    case CLEONOS_SYSCALL_SHUTDOWN:
        return "SHUTDOWN";
    case CLEONOS_SYSCALL_RESTART:
        return "RESTART";
    case CLEONOS_SYSCALL_AUDIO_AVAILABLE:
        return "AUDIO_AVAIL";
    case CLEONOS_SYSCALL_AUDIO_PLAY_TONE:
        return "AUDIO_TONE";
    case CLEONOS_SYSCALL_AUDIO_STOP:
        return "AUDIO_STOP";
    case CLEONOS_SYSCALL_EXEC_PATHV:
        return "EXEC_PATHV";
    case CLEONOS_SYSCALL_SPAWN_PATHV:
        return "SPAWN_PATHV";
    case CLEONOS_SYSCALL_PROC_ARGC:
        return "PROC_ARGC";
    case CLEONOS_SYSCALL_PROC_ARGV:
        return "PROC_ARGV";
    case CLEONOS_SYSCALL_PROC_ENVC:
        return "PROC_ENVC";
    case CLEONOS_SYSCALL_PROC_ENV:
        return "PROC_ENV";
    case CLEONOS_SYSCALL_PROC_LAST_SIGNAL:
        return "PROC_LAST_SIG";
    case CLEONOS_SYSCALL_PROC_FAULT_VECTOR:
        return "PROC_FAULT_VEC";
    case CLEONOS_SYSCALL_PROC_FAULT_ERROR:
        return "PROC_FAULT_ERR";
    case CLEONOS_SYSCALL_PROC_FAULT_RIP:
        return "PROC_FAULT_RIP";
    case CLEONOS_SYSCALL_PROC_COUNT:
        return "PROC_COUNT";
    case CLEONOS_SYSCALL_PROC_PID_AT:
        return "PROC_PID_AT";
    case CLEONOS_SYSCALL_PROC_SNAPSHOT:
        return "PROC_SNAPSHOT";
    case CLEONOS_SYSCALL_PROC_KILL:
        return "PROC_KILL";
    case CLEONOS_SYSCALL_KDBG_SYM:
        return "KDBG_SYM";
    case CLEONOS_SYSCALL_KDBG_BT:
        return "KDBG_BT";
    case CLEONOS_SYSCALL_KDBG_REGS:
        return "KDBG_REGS";
    case CLEONOS_SYSCALL_STATS_TOTAL:
        return "STATS_TOTAL";
    case CLEONOS_SYSCALL_STATS_ID_COUNT:
        return "STATS_ID_COUNT";
    case CLEONOS_SYSCALL_STATS_RECENT_WINDOW:
        return "STATS_RECENT_WIN";
    case CLEONOS_SYSCALL_STATS_RECENT_ID:
        return "STATS_RECENT_ID";
    default:
        return "UNKNOWN";
    }
}

static int ush_sysstat_next_token(const char **io_cursor, char *out, u64 out_size) {
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

static int ush_sysstat_parse_args(const char *arg, int *out_show_all, u64 *out_limit) {
    const char *cursor = arg;
    char token[USH_PATH_MAX];
    int show_all = 0;
    u64 limit = USH_SYSSTAT_DEFAULT_TOP;

    if (out_show_all == (int *)0 || out_limit == (u64 *)0) {
        return 0;
    }

    while (ush_sysstat_next_token(&cursor, token, (u64)sizeof(token)) != 0) {
        if (ush_streq(token, "-a") != 0 || ush_streq(token, "--all") != 0) {
            show_all = 1;
            continue;
        }

        if (ush_streq(token, "-n") != 0 || ush_streq(token, "--top") != 0) {
            if (ush_sysstat_next_token(&cursor, token, (u64)sizeof(token)) == 0 ||
                ush_parse_u64_dec(token, &limit) == 0) {
                return 0;
            }
            continue;
        }

        return 0;
    }

    if (limit == 0ULL) {
        limit = 1ULL;
    }

    *out_show_all = show_all;
    *out_limit = limit;
    return 1;
}

static void ush_sysstat_sort_recent(ush_sysstat_entry *entries, u64 count) {
    u64 i;

    if (entries == (ush_sysstat_entry *)0) {
        return;
    }

    for (i = 0ULL; i + 1ULL < count; i++) {
        u64 j;

        for (j = i + 1ULL; j < count; j++) {
            int swap = 0;

            if (entries[j].recent > entries[i].recent) {
                swap = 1;
            } else if (entries[j].recent == entries[i].recent && entries[j].total > entries[i].total) {
                swap = 1;
            } else if (entries[j].recent == entries[i].recent && entries[j].total == entries[i].total &&
                       entries[j].id < entries[i].id) {
                swap = 1;
            }

            if (swap != 0) {
                ush_sysstat_entry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
}

static int ush_cmd_sysstat(const char *arg) {
    int show_all = 0;
    u64 limit = USH_SYSSTAT_DEFAULT_TOP;
    ush_sysstat_entry entries[USH_SYSSTAT_MAX_IDS];
    u64 entry_count = 0ULL;
    u64 id;
    u64 total = cleonos_sys_stats_total();
    u64 recent_window = cleonos_sys_stats_recent_window();
    u64 to_show;

    if (ush_sysstat_parse_args(arg, &show_all, &limit) == 0) {
        ush_writeln("sysstat: usage sysstat [-a|--all] [-n N]");
        return 0;
    }

    ush_writeln("sysstat:");
    ush_print_kv_hex("  TIMER_TICKS", cleonos_sys_timer_ticks());
    ush_print_kv_hex("  TASK_COUNT", cleonos_sys_task_count());
    ush_print_kv_hex("  CURRENT_TASK", cleonos_syscall(CLEONOS_SYSCALL_CUR_TASK, 0ULL, 0ULL, 0ULL));
    ush_print_kv_hex("  CONTEXT_SWITCHES", cleonos_sys_context_switches());
    ush_print_kv_hex("  PROC_COUNT", cleonos_sys_proc_count());
    ush_print_kv_hex("  EXEC_REQUESTS", cleonos_sys_exec_request_count());
    ush_print_kv_hex("  EXEC_SUCCESS", cleonos_sys_exec_success_count());
    ush_print_kv_hex("  SYSCALL_TOTAL", total);
    ush_print_kv_hex("  SYSCALL_RECENT_WINDOW", recent_window);
    ush_writeln("");

    for (id = 0ULL; id < USH_SYSSTAT_MAX_IDS; id++) {
        u64 id_total = cleonos_sys_stats_id_count(id);
        u64 id_recent = cleonos_sys_stats_recent_id(id);

        if (show_all == 0 && id_total == 0ULL && id_recent == 0ULL) {
            continue;
        }

        entries[entry_count].id = id;
        entries[entry_count].recent = id_recent;
        entries[entry_count].total = id_total;
        entries[entry_count].name = ush_sysstat_name_for_id(id);
        entry_count++;
    }

    if (entry_count == 0ULL) {
        ush_writeln("(no syscall activity yet)");
        return 1;
    }

    if (show_all == 0) {
        ush_sysstat_sort_recent(entries, entry_count);
    }

    to_show = entry_count;
    if (show_all == 0 && to_show > limit) {
        to_show = limit;
    }

    for (id = 0ULL; id < to_show; id++) {
        ush_write("ID=");
        ush_write_hex_u64(entries[id].id);
        ush_write(" RECENT=");
        ush_write_hex_u64(entries[id].recent);
        ush_write(" TOTAL=");
        ush_write_hex_u64(entries[id].total);
        ush_write(" NAME=");
        ush_writeln(entries[id].name);
    }

    if (show_all == 0 && entry_count > to_show) {
        ush_write("... truncated, use ");
        ush_write("sysstat -a");
        ush_writeln(" to show all syscall IDs");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "sysstat") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_sysstat(arg);

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
