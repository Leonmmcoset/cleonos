#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/keyboard.h>
#include <clks/log.h>
#include <clks/shell.h>
#include <clks/string.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_SHELL_LINE_MAX      192U
#define CLKS_SHELL_CMD_MAX        32U
#define CLKS_SHELL_ARG_MAX       160U
#define CLKS_SHELL_NAME_MAX       96U
#define CLKS_SHELL_PATH_MAX      192U
#define CLKS_SHELL_CAT_LIMIT     512U
#define CLKS_SHELL_INPUT_BUDGET  128U
#define CLKS_SHELL_CLEAR_LINES    56U

static clks_bool clks_shell_ready = CLKS_FALSE;
static char clks_shell_line[CLKS_SHELL_LINE_MAX];
static usize clks_shell_line_len = 0U;

static clks_bool clks_shell_is_space(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_shell_is_printable(char ch) {
    return (ch >= 32 && ch <= 126) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_shell_streq(const char *left, const char *right) {
    return (clks_strcmp(left, right) == 0) ? CLKS_TRUE : CLKS_FALSE;
}

static void clks_shell_write(const char *text) {
    if (text == CLKS_NULL) {
        return;
    }

    clks_tty_write(text);
}

static void clks_shell_write_char(char ch) {
    clks_tty_write_char(ch);
}

static void clks_shell_writeln(const char *text) {
    clks_shell_write(text);
    clks_shell_write_char('\n');
}

static void clks_shell_prompt(void) {
    clks_shell_write("cleonos> ");
}

static void clks_shell_reset_line(void) {
    clks_shell_line_len = 0U;
    clks_shell_line[0] = '\0';
}

static void clks_shell_trim(char *line) {
    usize start = 0U;
    usize i = 0U;
    usize len;

    if (line == CLKS_NULL) {
        return;
    }

    while (line[start] != '\0' && clks_shell_is_space(line[start]) == CLKS_TRUE) {
        start++;
    }

    if (start > 0U) {
        while (line[start + i] != '\0') {
            line[i] = line[start + i];
            i++;
        }

        line[i] = '\0';
    }

    len = clks_strlen(line);

    while (len > 0U && clks_shell_is_space(line[len - 1U]) == CLKS_TRUE) {
        line[len - 1U] = '\0';
        len--;
    }
}

static void clks_shell_split_line(const char *line,
                                  char *out_cmd,
                                  usize out_cmd_size,
                                  char *out_arg,
                                  usize out_arg_size) {
    usize i = 0U;
    usize cmd_pos = 0U;
    usize arg_pos = 0U;

    if (line == CLKS_NULL || out_cmd == CLKS_NULL || out_arg == CLKS_NULL) {
        return;
    }

    out_cmd[0] = '\0';
    out_arg[0] = '\0';

    while (line[i] != '\0' && clks_shell_is_space(line[i]) == CLKS_TRUE) {
        i++;
    }

    while (line[i] != '\0' && clks_shell_is_space(line[i]) == CLKS_FALSE) {
        if (cmd_pos + 1U < out_cmd_size) {
            out_cmd[cmd_pos++] = line[i];
        }
        i++;
    }

    out_cmd[cmd_pos] = '\0';

    while (line[i] != '\0' && clks_shell_is_space(line[i]) == CLKS_TRUE) {
        i++;
    }

    while (line[i] != '\0') {
        if (arg_pos + 1U < out_arg_size) {
            out_arg[arg_pos++] = line[i];
        }
        i++;
    }

    out_arg[arg_pos] = '\0';
}

static clks_bool clks_shell_has_suffix(const char *name, const char *suffix) {
    usize name_len;
    usize suffix_len;

    if (name == CLKS_NULL || suffix == CLKS_NULL) {
        return CLKS_FALSE;
    }

    name_len = clks_strlen(name);
    suffix_len = clks_strlen(suffix);

    if (suffix_len > name_len) {
        return CLKS_FALSE;
    }

    return (clks_strcmp(name + (name_len - suffix_len), suffix) == 0) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_shell_resolve_exec_path(const char *arg, char *out_path, usize out_path_size) {
    usize cursor = 0U;
    usize i;

    if (arg == CLKS_NULL || out_path == CLKS_NULL || out_path_size == 0U) {
        return CLKS_FALSE;
    }

    if (arg[0] == '\0') {
        return CLKS_FALSE;
    }

    out_path[0] = '\0';

    if (arg[0] == '/') {
        usize len = clks_strlen(arg);
        if (len + 1U > out_path_size) {
            return CLKS_FALSE;
        }
        clks_memcpy(out_path, arg, len + 1U);
        return CLKS_TRUE;
    }

    {
        static const char shell_prefix[] = "/shell/";
        usize prefix_len = (sizeof(shell_prefix) - 1U);

        if (prefix_len + 1U >= out_path_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_path, shell_prefix, prefix_len);
        cursor = prefix_len;
    }

    for (i = 0U; arg[i] != '\0'; i++) {
        if (cursor + 1U >= out_path_size) {
            return CLKS_FALSE;
        }

        out_path[cursor++] = arg[i];
    }

    out_path[cursor] = '\0';

    if (clks_shell_has_suffix(out_path, ".elf") == CLKS_FALSE) {
        static const char elf_suffix[] = ".elf";
        usize suffix_len = (sizeof(elf_suffix) - 1U);

        if (cursor + suffix_len + 1U > out_path_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_path + cursor, elf_suffix, suffix_len + 1U);
    }

    return CLKS_TRUE;
}

static void clks_shell_cmd_help(void) {
    clks_shell_writeln("commands:");
    clks_shell_writeln("  help");
    clks_shell_writeln("  ls [dir]");
    clks_shell_writeln("  cat <file>");
    clks_shell_writeln("  exec <path|name>");
    clks_shell_writeln("  clear");
}

static void clks_shell_cmd_ls(const char *arg) {
    struct clks_fs_node_info info;
    const char *path = arg;
    u64 count;
    u64 i;

    if (path == CLKS_NULL || path[0] == '\0') {
        path = "/";
    }

    if (clks_fs_stat(path, &info) == CLKS_FALSE || info.type != CLKS_FS_NODE_DIR) {
        clks_shell_writeln("ls: directory not found");
        return;
    }

    count = clks_fs_count_children(path);

    if (count == 0ULL) {
        clks_shell_writeln("(empty)");
        return;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLKS_SHELL_NAME_MAX];

        name[0] = '\0';
        if (clks_fs_get_child_name(path, i, name, sizeof(name)) == CLKS_FALSE) {
            continue;
        }

        clks_shell_writeln(name);
    }
}

static void clks_shell_cmd_cat(const char *arg) {
    const void *data;
    u64 size = 0ULL;
    u64 copy_len;
    char out[CLKS_SHELL_CAT_LIMIT + 1U];

    if (arg == CLKS_NULL || arg[0] == '\0') {
        clks_shell_writeln("cat: file path required");
        return;
    }

    data = clks_fs_read_all(arg, &size);

    if (data == CLKS_NULL) {
        clks_shell_writeln("cat: file not found");
        return;
    }

    if (size == 0ULL) {
        clks_shell_writeln("(empty file)");
        return;
    }

    copy_len = (size < CLKS_SHELL_CAT_LIMIT) ? size : CLKS_SHELL_CAT_LIMIT;
    clks_memcpy(out, data, (usize)copy_len);
    out[copy_len] = '\0';

    clks_shell_writeln(out);

    if (size > copy_len) {
        clks_shell_writeln("[cat] output truncated");
    }
}

static void clks_shell_cmd_exec(const char *arg) {
    char path[CLKS_SHELL_PATH_MAX];
    u64 status = (u64)-1;

    if (clks_shell_resolve_exec_path(arg, path, sizeof(path)) == CLKS_FALSE) {
        clks_shell_writeln("exec: invalid target");
        return;
    }

    if (clks_exec_run_path(path, &status) == CLKS_TRUE && status == 0ULL) {
        clks_shell_writeln("exec: request accepted");
    } else {
        clks_shell_writeln("exec: request failed");
    }
}

static void clks_shell_cmd_clear(void) {
    u32 i;

    for (i = 0U; i < CLKS_SHELL_CLEAR_LINES; i++) {
        clks_shell_write_char('\n');
    }
}

static void clks_shell_execute_line(const char *line) {
    char line_buf[CLKS_SHELL_LINE_MAX];
    char cmd[CLKS_SHELL_CMD_MAX];
    char arg[CLKS_SHELL_ARG_MAX];
    usize i = 0U;

    if (line == CLKS_NULL) {
        return;
    }

    while (line[i] != '\0' && i + 1U < sizeof(line_buf)) {
        line_buf[i] = line[i];
        i++;
    }

    line_buf[i] = '\0';
    clks_shell_trim(line_buf);

    if (line_buf[0] == '\0') {
        return;
    }

    clks_shell_split_line(line_buf, cmd, sizeof(cmd), arg, sizeof(arg));

    if (clks_shell_streq(cmd, "help") == CLKS_TRUE) {
        clks_shell_cmd_help();
        return;
    }

    if (clks_shell_streq(cmd, "ls") == CLKS_TRUE) {
        clks_shell_cmd_ls(arg);
        return;
    }

    if (clks_shell_streq(cmd, "cat") == CLKS_TRUE) {
        clks_shell_cmd_cat(arg);
        return;
    }

    if (clks_shell_streq(cmd, "exec") == CLKS_TRUE || clks_shell_streq(cmd, "run") == CLKS_TRUE) {
        clks_shell_cmd_exec(arg);
        return;
    }

    if (clks_shell_streq(cmd, "clear") == CLKS_TRUE) {
        clks_shell_cmd_clear();
        return;
    }

    clks_shell_writeln("unknown command; type 'help'");
}

static void clks_shell_handle_char(char ch) {
    if (ch == '\r') {
        return;
    }

    if (ch == '\n') {
        clks_shell_write_char('\n');
        clks_shell_line[clks_shell_line_len] = '\0';
        clks_shell_execute_line(clks_shell_line);
        clks_shell_reset_line();
        clks_shell_prompt();
        return;
    }

    if (ch == '\b' || ch == 127) {
        if (clks_shell_line_len > 0U) {
            clks_shell_line_len--;
            clks_shell_line[clks_shell_line_len] = '\0';
            clks_shell_write_char('\b');
        }
        return;
    }

    if (ch == '\t') {
        ch = ' ';
    }

    if (clks_shell_is_printable(ch) == CLKS_FALSE) {
        return;
    }

    if (clks_shell_line_len + 1U >= CLKS_SHELL_LINE_MAX) {
        return;
    }

    clks_shell_line[clks_shell_line_len++] = ch;
    clks_shell_line[clks_shell_line_len] = '\0';
    clks_shell_write_char(ch);
}

void clks_shell_init(void) {
    clks_shell_reset_line();

    if (clks_tty_ready() == CLKS_FALSE) {
        clks_shell_ready = CLKS_FALSE;
        clks_log(CLKS_LOG_WARN, "SHELL", "TTY NOT READY; SHELL DISABLED");
        return;
    }

    clks_shell_ready = CLKS_TRUE;

    clks_shell_writeln("");
    clks_shell_writeln("CLeonOS interactive shell ready");
    clks_shell_writeln("type 'help' for commands");
    clks_shell_prompt();

    clks_log(CLKS_LOG_INFO, "SHELL", "INTERACTIVE LOOP ONLINE");
}

static void clks_shell_drain_input(u32 budget_limit) {
    u32 budget = 0U;
    char ch;

    if (budget_limit == 0U || clks_shell_ready == CLKS_FALSE) {
        return;
    }

    while (budget < budget_limit) {
        if (clks_keyboard_pop_char(&ch) == CLKS_FALSE) {
            break;
        }

        clks_shell_handle_char(ch);
        budget++;
    }
}

void clks_shell_pump_input(u32 max_chars) {
    clks_shell_drain_input(max_chars);
}

void clks_shell_tick(u64 tick) {
    (void)tick;
    clks_shell_drain_input(CLKS_SHELL_INPUT_BUDGET);
}