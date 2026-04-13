#include "shell_internal.h"

#define USH_DMESG_DEFAULT   64ULL
#define USH_DMESG_LINE_MAX 256ULL
#define USH_COPY_MAX      65536U

static int ush_path_is_under_temp(const char *path) {
    if (path == (const char *)0) {
        return 0;
    }

    if (path[0] != '/' || path[1] != 't' || path[2] != 'e' || path[3] != 'm' || path[4] != 'p') {
        return 0;
    }

    return (path[5] == '\0' || path[5] == '/') ? 1 : 0;
}

static int ush_split_first_and_rest(const char *arg, char *out_first, u64 out_first_size, const char **out_rest) {
    u64 i = 0ULL;
    u64 p = 0ULL;

    if (arg == (const char *)0 || out_first == (char *)0 || out_first_size == 0ULL || out_rest == (const char **)0) {
        return 0;
    }

    out_first[0] = '\0';
    *out_rest = "";

    while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
        i++;
    }

    if (arg[i] == '\0') {
        return 0;
    }

    while (arg[i] != '\0' && ush_is_space(arg[i]) == 0) {
        if (p + 1ULL < out_first_size) {
            out_first[p++] = arg[i];
        }
        i++;
    }

    out_first[p] = '\0';

    while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
        i++;
    }

    *out_rest = &arg[i];
    return 1;
}

static int ush_split_two_args(const char *arg,
                              char *out_first,
                              u64 out_first_size,
                              char *out_second,
                              u64 out_second_size) {
    u64 i = 0ULL;
    u64 p = 0ULL;

    if (arg == (const char *)0 ||
        out_first == (char *)0 || out_first_size == 0ULL ||
        out_second == (char *)0 || out_second_size == 0ULL) {
        return 0;
    }

    out_first[0] = '\0';
    out_second[0] = '\0';

    while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
        i++;
    }

    if (arg[i] == '\0') {
        return 0;
    }

    while (arg[i] != '\0' && ush_is_space(arg[i]) == 0) {
        if (p + 1ULL < out_first_size) {
            out_first[p++] = arg[i];
        }
        i++;
    }

    out_first[p] = '\0';

    while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
        i++;
    }

    if (arg[i] == '\0') {
        return 0;
    }

    p = 0ULL;
    while (arg[i] != '\0' && ush_is_space(arg[i]) == 0) {
        if (p + 1ULL < out_second_size) {
            out_second[p++] = arg[i];
        }
        i++;
    }

    out_second[p] = '\0';

    return (out_first[0] != '\0' && out_second[0] != '\0') ? 1 : 0;
}

static int ush_cmd_help(void) {
    ush_writeln("commands:");
    ush_writeln("  help");
    ush_writeln("  ls [dir]");
    ush_writeln("  cat <file>");
    ush_writeln("  pwd");
    ush_writeln("  cd [dir]");
    ush_writeln("  exec|run <path|name>");
    ush_writeln("  clear");
    ush_writeln("  memstat / fsstat / taskstat / userstat / shstat / stats");
    ush_writeln("  tty [index]");
    ush_writeln("  dmesg [n]");
    ush_writeln("  kbdstat");
    ush_writeln("  mkdir <dir>      (/temp only)");
    ush_writeln("  touch <file>     (/temp only)");
    ush_writeln("  write <file> <text>   (/temp only)");
    ush_writeln("  append <file> <text>  (/temp only)");
    ush_writeln("  cp <src> <dst>   (dst /temp only)");
    ush_writeln("  mv <src> <dst>   (/temp only)");
    ush_writeln("  rm <path>        (/temp only)");
    ush_writeln("  pid");
    ush_writeln("  spawn <path|name>");
    ush_writeln("  wait <pid>");
    ush_writeln("  sleep <ticks>");
    ush_writeln("  yield");
    ush_writeln("  exit [code]");
    ush_writeln("  rusttest / panic / elfloader (kernel shell only)");
    ush_writeln("edit keys: Left/Right, Home/End, Up/Down history");
    return 1;
}

static int ush_cmd_ls(const ush_state *sh, const char *arg) {
    const char *target = arg;
    char path[USH_PATH_MAX];
    u64 count;
    u64 i;

    if (target == (const char *)0 || target[0] == '\0') {
        target = ".";
    }

    if (ush_resolve_path(sh, target, path, (u64)sizeof(path)) == 0) {
        ush_writeln("ls: invalid path");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(path) != 2ULL) {
        ush_writeln("ls: directory not found");
        return 0;
    }

    count = cleonos_sys_fs_child_count(path);

    if (count == 0ULL) {
        ush_writeln("(empty)");
        return 1;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        name[0] = '\0';

        if (cleonos_sys_fs_get_child_name(path, i, name) != 0ULL) {
            ush_writeln(name);
        }
    }

    return 1;
}

static int ush_cmd_cat(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];
    char buf[USH_CAT_MAX + 1ULL];
    u64 size;
    u64 req;
    u64 got;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("cat: file path required");
        return 0;
    }

    if (ush_resolve_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln("cat: invalid path");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(path) != 1ULL) {
        ush_writeln("cat: file not found");
        return 0;
    }

    size = cleonos_sys_fs_stat_size(path);

    if (size == (u64)-1) {
        ush_writeln("cat: failed to stat file");
        return 0;
    }

    if (size == 0ULL) {
        return 1;
    }

    req = (size < USH_CAT_MAX) ? size : USH_CAT_MAX;
    got = cleonos_sys_fs_read(path, buf, req);

    if (got == 0ULL) {
        ush_writeln("cat: read failed");
        return 0;
    }

    if (got > USH_CAT_MAX) {
        got = USH_CAT_MAX;
    }

    buf[got] = '\0';
    ush_writeln(buf);

    if (size > got) {
        ush_writeln("[cat] output truncated");
    }

    return 1;
}

static int ush_cmd_pwd(const ush_state *sh) {
    ush_writeln(sh->cwd);
    return 1;
}

static int ush_cmd_cd(ush_state *sh, const char *arg) {
    const char *target = arg;
    char path[USH_PATH_MAX];

    if (target == (const char *)0 || target[0] == '\0') {
        target = "/";
    }

    if (ush_resolve_path(sh, target, path, (u64)sizeof(path)) == 0) {
        ush_writeln("cd: invalid path");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(path) != 2ULL) {
        ush_writeln("cd: directory not found");
        return 0;
    }

    ush_copy(sh->cwd, (u64)sizeof(sh->cwd), path);
    return 1;
}

static int ush_cmd_exec(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];
    u64 status;

    if (ush_resolve_exec_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln("exec: invalid target");
        return 0;
    }

    if (ush_path_is_under_system(path) != 0) {
        ush_writeln("exec: /system/*.elf is kernel-mode (KELF), not user-exec");
        return 0;
    }

    status = cleonos_sys_exec_path(path);

    if (status == (u64)-1) {
        ush_writeln("exec: request failed");
        return 0;
    }

    if (status == 0ULL) {
        ush_writeln("exec: request accepted");
        return 1;
    }

    ush_writeln("exec: returned non-zero status");
    return 0;
}

static int ush_cmd_pid(void) {
    ush_print_kv_hex("PID", cleonos_sys_getpid());
    return 1;
}

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

static int ush_cmd_wait(const char *arg) {
    u64 pid;
    u64 status = (u64)-1;
    u64 wait_ret;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("wait: usage wait <pid>");
        return 0;
    }

    if (ush_parse_u64_dec(arg, &pid) == 0) {
        ush_writeln("wait: invalid pid");
        return 0;
    }

    wait_ret = cleonos_sys_wait_pid(pid, &status);

    if (wait_ret == (u64)-1) {
        ush_writeln("wait: pid not found");
        return 0;
    }

    if (wait_ret == 0ULL) {
        ush_writeln("wait: still running");
        return 1;
    }

    ush_writeln("wait: exited");
    ush_print_kv_hex("  STATUS", status);
    return 1;
}

static int ush_cmd_sleep(const char *arg) {
    u64 ticks;
    u64 elapsed;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("sleep: usage sleep <ticks>");
        return 0;
    }

    if (ush_parse_u64_dec(arg, &ticks) == 0) {
        ush_writeln("sleep: invalid ticks");
        return 0;
    }

    elapsed = cleonos_sys_sleep_ticks(ticks);
    ush_print_kv_hex("SLEPT_TICKS", elapsed);
    return 1;
}

static int ush_cmd_yield(void) {
    ush_print_kv_hex("YIELD_TICK", cleonos_sys_yield());
    return 1;
}

static int ush_cmd_exit(ush_state *sh, const char *arg) {
    u64 code = 0ULL;

    if (sh == (ush_state *)0) {
        return 0;
    }

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_parse_u64_dec(arg, &code) == 0) {
            ush_writeln("exit: usage exit [code]");
            return 0;
        }
    }

    sh->exit_requested = 1;
    sh->exit_code = code;
    (void)cleonos_sys_exit(code);
    ush_writeln("exit: shell stopping");
    return 1;
}

static int ush_cmd_clear(void) {
    u64 i;

    for (i = 0ULL; i < USH_CLEAR_LINES; i++) {
        ush_write_char('\n');
    }

    return 1;
}

static int ush_cmd_kbdstat(void) {
    ush_writeln("kbdstat:");
    ush_print_kv_hex("  BUFFERED", cleonos_sys_kbd_buffered());
    ush_print_kv_hex("  PUSHED", cleonos_sys_kbd_pushed());
    ush_print_kv_hex("  POPPED", cleonos_sys_kbd_popped());
    ush_print_kv_hex("  DROPPED", cleonos_sys_kbd_dropped());
    ush_print_kv_hex("  HOTKEY_SWITCHES", cleonos_sys_kbd_hotkey_switches());
    return 1;
}

static int ush_cmd_memstat(void) {
    ush_writeln("memstat (user ABI limited):");
    ush_print_kv_hex("  SERVICE_COUNT", cleonos_sys_service_count());
    ush_print_kv_hex("  SERVICE_READY_COUNT", cleonos_sys_service_ready_count());
    ush_print_kv_hex("  KELF_COUNT", cleonos_sys_kelf_count());
    ush_print_kv_hex("  KELF_RUNS", cleonos_sys_kelf_runs());
    return 1;
}

static int ush_cmd_fsstat(void) {
    ush_writeln("fsstat:");
    ush_print_kv_hex("  NODE_COUNT", cleonos_sys_fs_node_count());
    ush_print_kv_hex("  ROOT_CHILDREN", cleonos_sys_fs_child_count("/"));
    ush_print_kv_hex("  SYSTEM_CHILDREN", cleonos_sys_fs_child_count("/system"));
    ush_print_kv_hex("  SHELL_CHILDREN", cleonos_sys_fs_child_count("/shell"));
    ush_print_kv_hex("  TEMP_CHILDREN", cleonos_sys_fs_child_count("/temp"));
    ush_print_kv_hex("  DRIVER_CHILDREN", cleonos_sys_fs_child_count("/driver"));
    return 1;
}

static int ush_cmd_taskstat(void) {
    ush_writeln("taskstat:");
    ush_print_kv_hex("  TASK_COUNT", cleonos_sys_task_count());
    ush_print_kv_hex("  CURRENT_TASK", cleonos_syscall(CLEONOS_SYSCALL_CUR_TASK, 0ULL, 0ULL, 0ULL));
    ush_print_kv_hex("  TIMER_TICKS", cleonos_sys_timer_ticks());
    ush_print_kv_hex("  CONTEXT_SWITCHES", cleonos_sys_context_switches());
    return 1;
}

static int ush_cmd_userstat(void) {
    ush_writeln("userstat:");
    ush_print_kv_hex("  USER_SHELL_READY", cleonos_sys_user_shell_ready());
    ush_print_kv_hex("  USER_EXEC_REQUESTED", cleonos_sys_user_exec_requested());
    ush_print_kv_hex("  USER_LAUNCH_TRIES", cleonos_sys_user_launch_tries());
    ush_print_kv_hex("  USER_LAUNCH_OK", cleonos_sys_user_launch_ok());
    ush_print_kv_hex("  USER_LAUNCH_FAIL", cleonos_sys_user_launch_fail());
    ush_print_kv_hex("  EXEC_REQUESTS", cleonos_sys_exec_request_count());
    ush_print_kv_hex("  EXEC_SUCCESS", cleonos_sys_exec_success_count());
    ush_print_kv_hex("  TTY_COUNT", cleonos_sys_tty_count());
    ush_print_kv_hex("  TTY_ACTIVE", cleonos_sys_tty_active());
    return 1;
}

static int ush_cmd_shstat(const ush_state *sh) {
    ush_writeln("shstat:");
    ush_print_kv_hex("  CMD_TOTAL", sh->cmd_total);
    ush_print_kv_hex("  CMD_OK", sh->cmd_ok);
    ush_print_kv_hex("  CMD_FAIL", sh->cmd_fail);
    ush_print_kv_hex("  CMD_UNKNOWN", sh->cmd_unknown);
    ush_print_kv_hex("  EXIT_REQUESTED", (sh->exit_requested != 0) ? 1ULL : 0ULL);
    ush_print_kv_hex("  EXIT_CODE", sh->exit_code);
    return 1;
}

static int ush_cmd_tty(const char *arg) {
    u64 tty_count = cleonos_sys_tty_count();
    u64 active = cleonos_sys_tty_active();

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_print_kv_hex("TTY_COUNT", tty_count);
        ush_print_kv_hex("TTY_ACTIVE", active);
        return 1;
    }

    {
        u64 idx;

        if (ush_parse_u64_dec(arg, &idx) == 0) {
            ush_writeln("tty: usage tty [index]");
            return 0;
        }

        if (idx >= tty_count) {
            ush_writeln("tty: index out of range");
            return 0;
        }

        if (cleonos_sys_tty_switch(idx) == (u64)-1) {
            ush_writeln("tty: switch failed");
            return 0;
        }

        ush_writeln("tty: switched");
        ush_print_kv_hex("TTY_ACTIVE", cleonos_sys_tty_active());
        return 1;
    }
}

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

static int ush_cmd_mkdir(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("mkdir: directory path required");
        return 0;
    }

    if (ush_resolve_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln("mkdir: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(path) == 0) {
        ush_writeln("mkdir: target must be under /temp");
        return 0;
    }

    if (cleonos_sys_fs_mkdir(path) == 0ULL) {
        ush_writeln("mkdir: failed");
        return 0;
    }

    return 1;
}

static int ush_cmd_touch(const ush_state *sh, const char *arg) {
    static const char empty_data[1] = {'\0'};
    char path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("touch: file path required");
        return 0;
    }

    if (ush_resolve_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln("touch: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(path) == 0) {
        ush_writeln("touch: target must be under /temp");
        return 0;
    }

    if (cleonos_sys_fs_write(path, empty_data, 0ULL) == 0ULL) {
        ush_writeln("touch: failed");
        return 0;
    }

    return 1;
}

static int ush_cmd_write(const ush_state *sh, const char *arg) {
    char path_arg[USH_PATH_MAX];
    char abs_path[USH_PATH_MAX];
    const char *payload;
    u64 payload_len;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("write: usage write <file> <text>");
        return 0;
    }

    if (ush_split_first_and_rest(arg, path_arg, (u64)sizeof(path_arg), &payload) == 0) {
        ush_writeln("write: usage write <file> <text>");
        return 0;
    }

    if (ush_resolve_path(sh, path_arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln("write: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(abs_path) == 0) {
        ush_writeln("write: target must be under /temp");
        return 0;
    }

    payload_len = ush_strlen(payload);

    if (cleonos_sys_fs_write(abs_path, payload, payload_len) == 0ULL) {
        ush_writeln("write: failed");
        return 0;
    }

    return 1;
}

static int ush_cmd_append(const ush_state *sh, const char *arg) {
    char path_arg[USH_PATH_MAX];
    char abs_path[USH_PATH_MAX];
    const char *payload;
    u64 payload_len;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("append: usage append <file> <text>");
        return 0;
    }

    if (ush_split_first_and_rest(arg, path_arg, (u64)sizeof(path_arg), &payload) == 0) {
        ush_writeln("append: usage append <file> <text>");
        return 0;
    }

    if (ush_resolve_path(sh, path_arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln("append: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(abs_path) == 0) {
        ush_writeln("append: target must be under /temp");
        return 0;
    }

    payload_len = ush_strlen(payload);

    if (cleonos_sys_fs_append(abs_path, payload, payload_len) == 0ULL) {
        ush_writeln("append: failed");
        return 0;
    }

    return 1;
}

static int ush_copy_file(const char *src_path, const char *dst_path) {
    static char copy_buf[USH_COPY_MAX];
    u64 src_type;
    u64 src_size;
    u64 got;

    src_type = cleonos_sys_fs_stat_type(src_path);

    if (src_type != 1ULL) {
        ush_writeln("cp: source file not found");
        return 0;
    }

    src_size = cleonos_sys_fs_stat_size(src_path);

    if (src_size == (u64)-1) {
        ush_writeln("cp: failed to stat source");
        return 0;
    }

    if (src_size > (u64)USH_COPY_MAX) {
        ush_writeln("cp: source too large for user shell buffer");
        return 0;
    }

    if (src_size == 0ULL) {
        got = 0ULL;
    } else {
        got = cleonos_sys_fs_read(src_path, copy_buf, src_size);

        if (got == 0ULL || got != src_size) {
            ush_writeln("cp: failed to read source");
            return 0;
        }
    }

    if (cleonos_sys_fs_write(dst_path, copy_buf, got) == 0ULL) {
        ush_writeln("cp: failed to write destination");
        return 0;
    }

    return 1;
}

static int ush_cmd_cp(const ush_state *sh, const char *arg) {
    char src_arg[USH_PATH_MAX];
    char dst_arg[USH_PATH_MAX];
    char src_path[USH_PATH_MAX];
    char dst_path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("cp: usage cp <src> <dst>");
        return 0;
    }

    if (ush_split_two_args(arg, src_arg, (u64)sizeof(src_arg), dst_arg, (u64)sizeof(dst_arg)) == 0) {
        ush_writeln("cp: usage cp <src> <dst>");
        return 0;
    }

    if (ush_resolve_path(sh, src_arg, src_path, (u64)sizeof(src_path)) == 0 ||
        ush_resolve_path(sh, dst_arg, dst_path, (u64)sizeof(dst_path)) == 0) {
        ush_writeln("cp: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(dst_path) == 0) {
        ush_writeln("cp: destination must be under /temp");
        return 0;
    }

    return ush_copy_file(src_path, dst_path);
}

static int ush_cmd_mv(const ush_state *sh, const char *arg) {
    char src_arg[USH_PATH_MAX];
    char dst_arg[USH_PATH_MAX];
    char src_path[USH_PATH_MAX];
    char dst_path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("mv: usage mv <src> <dst>");
        return 0;
    }

    if (ush_split_two_args(arg, src_arg, (u64)sizeof(src_arg), dst_arg, (u64)sizeof(dst_arg)) == 0) {
        ush_writeln("mv: usage mv <src> <dst>");
        return 0;
    }

    if (ush_resolve_path(sh, src_arg, src_path, (u64)sizeof(src_path)) == 0 ||
        ush_resolve_path(sh, dst_arg, dst_path, (u64)sizeof(dst_path)) == 0) {
        ush_writeln("mv: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(src_path) == 0 || ush_path_is_under_temp(dst_path) == 0) {
        ush_writeln("mv: source and destination must be under /temp");
        return 0;
    }

    if (ush_copy_file(src_path, dst_path) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_remove(src_path) == 0ULL) {
        ush_writeln("mv: source remove failed");
        return 0;
    }

    return 1;
}

static int ush_cmd_rm(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("rm: path required");
        return 0;
    }

    if (ush_resolve_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln("rm: invalid path");
        return 0;
    }

    if (ush_path_is_under_temp(path) == 0) {
        ush_writeln("rm: target must be under /temp");
        return 0;
    }

    if (cleonos_sys_fs_remove(path) == 0ULL) {
        ush_writeln("rm: failed (directory must be empty)");
        return 0;
    }

    return 1;
}

static int ush_cmd_stats(const ush_state *sh) {
    (void)ush_cmd_memstat();
    (void)ush_cmd_fsstat();
    (void)ush_cmd_taskstat();
    (void)ush_cmd_userstat();
    (void)ush_cmd_kbdstat();
    (void)ush_cmd_shstat(sh);
    return 1;
}

static int ush_cmd_not_supported(const char *name, const char *why) {
    ush_write(name);
    ush_write(": ");
    ush_writeln(why);
    return 0;
}

void ush_execute_line(ush_state *sh, const char *line) {
    char line_buf[USH_LINE_MAX];
    char cmd[USH_CMD_MAX];
    char arg[USH_ARG_MAX];
    u64 i = 0ULL;
    int known = 1;
    int success = 0;

    if (sh == (ush_state *)0 || line == (const char *)0) {
        return;
    }

    while (line[i] != '\0' && i + 1ULL < (u64)sizeof(line_buf)) {
        line_buf[i] = line[i];
        i++;
    }

    line_buf[i] = '\0';
    ush_trim_line(line_buf);

    if (line_buf[0] == '\0' || line_buf[0] == '#') {
        return;
    }

    ush_parse_line(line_buf, cmd, (u64)sizeof(cmd), arg, (u64)sizeof(arg));
    ush_trim_line(arg);

    if (ush_streq(cmd, "help") != 0) {
        success = ush_cmd_help();
    } else if (ush_streq(cmd, "ls") != 0 || ush_streq(cmd, "dir") != 0) {
        success = ush_cmd_ls(sh, arg);
    } else if (ush_streq(cmd, "cat") != 0) {
        success = ush_cmd_cat(sh, arg);
    } else if (ush_streq(cmd, "pwd") != 0) {
        success = ush_cmd_pwd(sh);
    } else if (ush_streq(cmd, "cd") != 0) {
        success = ush_cmd_cd(sh, arg);
    } else if (ush_streq(cmd, "exec") != 0 || ush_streq(cmd, "run") != 0) {
        success = ush_cmd_exec(sh, arg);
    } else if (ush_streq(cmd, "pid") != 0) {
        success = ush_cmd_pid();
    } else if (ush_streq(cmd, "spawn") != 0) {
        success = ush_cmd_spawn(sh, arg);
    } else if (ush_streq(cmd, "wait") != 0) {
        success = ush_cmd_wait(arg);
    } else if (ush_streq(cmd, "sleep") != 0) {
        success = ush_cmd_sleep(arg);
    } else if (ush_streq(cmd, "yield") != 0) {
        success = ush_cmd_yield();
    } else if (ush_streq(cmd, "exit") != 0) {
        success = ush_cmd_exit(sh, arg);
    } else if (ush_streq(cmd, "clear") != 0 || ush_streq(cmd, "cls") != 0) {
        success = ush_cmd_clear();
    } else if (ush_streq(cmd, "memstat") != 0) {
        success = ush_cmd_memstat();
    } else if (ush_streq(cmd, "fsstat") != 0) {
        success = ush_cmd_fsstat();
    } else if (ush_streq(cmd, "taskstat") != 0) {
        success = ush_cmd_taskstat();
    } else if (ush_streq(cmd, "userstat") != 0) {
        success = ush_cmd_userstat();
    } else if (ush_streq(cmd, "shstat") != 0) {
        success = ush_cmd_shstat(sh);
    } else if (ush_streq(cmd, "stats") != 0) {
        success = ush_cmd_stats(sh);
    } else if (ush_streq(cmd, "tty") != 0) {
        success = ush_cmd_tty(arg);
    } else if (ush_streq(cmd, "dmesg") != 0) {
        success = ush_cmd_dmesg(arg);
    } else if (ush_streq(cmd, "kbdstat") != 0) {
        success = ush_cmd_kbdstat();
    } else if (ush_streq(cmd, "mkdir") != 0) {
        success = ush_cmd_mkdir(sh, arg);
    } else if (ush_streq(cmd, "touch") != 0) {
        success = ush_cmd_touch(sh, arg);
    } else if (ush_streq(cmd, "write") != 0) {
        success = ush_cmd_write(sh, arg);
    } else if (ush_streq(cmd, "append") != 0) {
        success = ush_cmd_append(sh, arg);
    } else if (ush_streq(cmd, "cp") != 0) {
        success = ush_cmd_cp(sh, arg);
    } else if (ush_streq(cmd, "mv") != 0) {
        success = ush_cmd_mv(sh, arg);
    } else if (ush_streq(cmd, "rm") != 0) {
        success = ush_cmd_rm(sh, arg);
    } else if (ush_streq(cmd, "rusttest") != 0 || ush_streq(cmd, "panic") != 0 || ush_streq(cmd, "elfloader") != 0) {
        success = ush_cmd_not_supported(cmd, "this command is kernel-shell only");
    } else {
        known = 0;
        success = 0;
        ush_writeln("unknown command; type 'help'");
    }

    sh->cmd_total++;

    if (success != 0) {
        sh->cmd_ok++;
    } else {
        sh->cmd_fail++;
    }

    if (known == 0) {
        sh->cmd_unknown++;
    }
}