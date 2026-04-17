#include "shell_internal.h"

#define USH_DMESG_DEFAULT   64ULL
#define USH_DMESG_LINE_MAX 256ULL
#define USH_COPY_MAX      65536U
#define USH_PIPELINE_MAX_STAGES 8ULL
#define USH_PIPE_CAPTURE_MAX   USH_COPY_MAX
#define USH_SORT_MAX_LINES 4096ULL

typedef struct ush_pipeline_stage {
    char text[USH_LINE_MAX];
    char cmd[USH_CMD_MAX];
    char arg[USH_ARG_MAX];
    char redirect_path[USH_PATH_MAX];
    int redirect_mode; /* 0: none, 1: >, 2: >> */
} ush_pipeline_stage;

static const char *ush_pipeline_stdin_text = (const char *)0;
static u64 ush_pipeline_stdin_len = 0ULL;
static char ush_pipeline_capture_a[USH_PIPE_CAPTURE_MAX + 1U];
static char ush_pipeline_capture_b[USH_PIPE_CAPTURE_MAX + 1U];


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
    ush_writeln("  args [a b c]      (print argc/argv/envp)");
    ush_writeln("  ls [-l] [-R] [path]");
    ush_writeln("  cat [file]        (reads pipeline input when file omitted)");
    ush_writeln("  grep [-n] <pattern> [file]");
    ush_writeln("  head [-n N] [file] / tail [-n N] [file]");
    ush_writeln("  wc [file] / cut -d <char> -f <N> [file] / uniq [file] / sort [file]");
    ush_writeln("  pwd");
    ush_writeln("  cd [dir]");
    ush_writeln("  exec|run <path|name> [args...]");
    ush_writeln("  clear");
    ush_writeln("  ansi / ansitest / color");
    ush_writeln("  wavplay <file.wav> [steps] [ticks] / wavplay --stop");
    ush_writeln("  fastfetch [--plain]");
    ush_writeln("  memstat / fsstat / taskstat / userstat / shstat / stats");
    ush_writeln("  tty [index]");
    ush_writeln("  dmesg [n]");
    ush_writeln("  kbdstat");
    ush_writeln("  mkdir <dir>      (/temp only)");
    ush_writeln("  touch <file>     (/temp only)");
    ush_writeln("  write <file> <text>   (/temp only, or from pipeline)");
    ush_writeln("  append <file> <text>  (/temp only, or from pipeline)");
    ush_writeln("  cp <src> <dst>   (dst /temp only)");
    ush_writeln("  mv <src> <dst>   (/temp only)");
    ush_writeln("  rm <path>        (/temp only)");
    ush_writeln("  pid");
    ush_writeln("  spawn <path|name> [args...] / bg <path|name> [args...]");
    ush_writeln("  wait <pid> / fg [pid]");
    ush_writeln("  kill <pid> [signal]");
    ush_writeln("  jobs [-a] / ps [-a] [-u] / top [--once] [-n loops] [-d ticks]");
    ush_writeln("  sleep <ticks>");
    ush_writeln("  yield");
    ush_writeln("  shutdown / restart");
    ush_writeln("  exit [code]");
    ush_writeln("  rusttest / panic / elfloader (kernel shell only)");
    ush_writeln("pipeline/redirection: cmd1 | cmd2 | cmd3 > /temp/out.txt");
    ush_writeln("redirection append:   cmd >> /temp/out.txt");
    ush_writeln("edit keys: Left/Right, Home/End, Up/Down history");
    return 1;
}

static int ush_ls_join_path(const char *dir_path, const char *name, char *out_path, u64 out_size) {
    u64 p = 0ULL;
    u64 i;

    if (dir_path == (const char *)0 || name == (const char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (dir_path[0] == '/' && dir_path[1] == '\0') {
        if (out_size < 2ULL) {
            return 0;
        }

        out_path[p++] = '/';
    } else {
        for (i = 0ULL; dir_path[i] != '\0'; i++) {
            if (p + 1ULL >= out_size) {
                return 0;
            }

            out_path[p++] = dir_path[i];
        }

        if (p == 0ULL || out_path[p - 1ULL] != '/') {
            if (p + 1ULL >= out_size) {
                return 0;
            }

            out_path[p++] = '/';
        }
    }

    for (i = 0ULL; name[i] != '\0'; i++) {
        if (p + 1ULL >= out_size) {
            return 0;
        }

        out_path[p++] = name[i];
    }

    out_path[p] = '\0';
    return 1;
}

static const char *ush_ls_basename(const char *path) {
    const char *name = path;
    u64 i = 0ULL;

    if (path == (const char *)0 || path[0] == '\0') {
        return "";
    }

    while (path[i] != '\0') {
        if (path[i] == '/' && path[i + 1ULL] != '\0') {
            name = &path[i + 1ULL];
        }

        i++;
    }

    return name;
}

static int ush_ls_is_dot_entry(const char *name) {
    if (name == (const char *)0) {
        return 0;
    }

    if (name[0] == '.' && name[1] == '\0') {
        return 1;
    }

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return 1;
    }

    return 0;
}

static void ush_ls_print_one(const char *name, u64 type, u64 size, int long_mode) {
    if (long_mode == 0) {
        ush_writeln(name);
        return;
    }

    if (type == 2ULL) {
        ush_write("d ");
    } else if (type == 1ULL) {
        ush_write("f ");
    } else {
        ush_write("? ");
    }

    ush_write(name);

    if (type == 1ULL) {
        ush_write("  size=");
        ush_write_hex_u64(size);
    } else if (type == 2ULL) {
        ush_write("  <DIR>");
    } else {
        ush_write("  <UNKNOWN>");
    }

    ush_write_char('\n');
}

static int ush_ls_parse_args(const char *arg,
                             int *out_long_mode,
                             int *out_recursive,
                             char *out_target,
                             u64 out_target_size) {
    char token[USH_PATH_MAX];
    u64 i = 0ULL;
    int path_set = 0;

    if (out_long_mode == (int *)0 ||
        out_recursive == (int *)0 ||
        out_target == (char *)0 ||
        out_target_size == 0ULL) {
        return 0;
    }

    *out_long_mode = 0;
    *out_recursive = 0;
    ush_copy(out_target, out_target_size, ".");

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    while (arg[i] != '\0') {
        u64 p = 0ULL;
        u64 j;

        while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
            i++;
        }

        if (arg[i] == '\0') {
            break;
        }

        while (arg[i] != '\0' && ush_is_space(arg[i]) == 0) {
            if (p + 1ULL < (u64)sizeof(token)) {
                token[p++] = arg[i];
            }

            i++;
        }

        token[p] = '\0';

        if (token[0] == '-' && token[1] != '\0') {
            for (j = 1ULL; token[j] != '\0'; j++) {
                if (token[j] == 'l') {
                    *out_long_mode = 1;
                } else if (token[j] == 'R') {
                    *out_recursive = 1;
                } else {
                    return 0;
                }
            }

            continue;
        }

        if (path_set != 0) {
            return 0;
        }

        ush_copy(out_target, out_target_size, token);
        path_set = 1;
    }

    return 1;
}

static int ush_ls_dir(const char *path,
                      int long_mode,
                      int recursive,
                      int print_header,
                      u64 depth) {
    u64 count;
    u64 i;

    if (depth > 16ULL) {
        ush_writeln("ls: recursion depth limit reached");
        return 0;
    }

    count = cleonos_sys_fs_child_count(path);

    if (print_header != 0) {
        ush_write(path);
        ush_writeln(":");
    }

    if (count == 0ULL) {
        ush_writeln("(empty)");
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_path[USH_PATH_MAX];
        u64 type;
        u64 size = 0ULL;

        name[0] = '\0';

        if (cleonos_sys_fs_get_child_name(path, i, name) == 0ULL) {
            continue;
        }

        if (ush_ls_join_path(path, name, child_path, (u64)sizeof(child_path)) == 0) {
            continue;
        }

        type = cleonos_sys_fs_stat_type(child_path);

        if (type == 1ULL) {
            size = cleonos_sys_fs_stat_size(child_path);
        }

        ush_ls_print_one(name, type, size, long_mode);
    }

    if (recursive == 0) {
        return 1;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_path[USH_PATH_MAX];

        name[0] = '\0';

        if (cleonos_sys_fs_get_child_name(path, i, name) == 0ULL) {
            continue;
        }

        if (ush_ls_is_dot_entry(name) != 0) {
            continue;
        }

        if (ush_ls_join_path(path, name, child_path, (u64)sizeof(child_path)) == 0) {
            continue;
        }

        if (cleonos_sys_fs_stat_type(child_path) == 2ULL) {
            ush_write_char('\n');
            (void)ush_ls_dir(child_path, long_mode, recursive, 1, depth + 1ULL);
        }
    }

    return 1;
}

static int ush_cmd_ls(const ush_state *sh, const char *arg) {
    char target[USH_PATH_MAX];
    char path[USH_PATH_MAX];
    u64 type;
    int long_mode;
    int recursive;

    if (ush_ls_parse_args(arg, &long_mode, &recursive, target, (u64)sizeof(target)) == 0) {
        ush_writeln("ls: usage ls [-l] [-R] [path]");
        return 0;
    }

    if (ush_resolve_path(sh, target, path, (u64)sizeof(path)) == 0) {
        ush_writeln("ls: invalid path");
        return 0;
    }

    type = cleonos_sys_fs_stat_type(path);

    if (type == 1ULL) {
        u64 size = cleonos_sys_fs_stat_size(path);
        ush_ls_print_one(ush_ls_basename(path), type, size, long_mode);
        return 1;
    }

    if (type != 2ULL) {
        ush_writeln("ls: path not found");
        return 0;
    }

    return ush_ls_dir(path, long_mode, recursive, recursive, 0ULL);
}

static int ush_cmd_cat(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];
    char buf[USH_CAT_MAX + 1ULL];
    u64 size;
    u64 req;
    u64 got;

    if (arg == (const char *)0 || arg[0] == '\0') {
        if (ush_pipeline_stdin_text != (const char *)0 && ush_pipeline_stdin_len > 0ULL) {
            ush_write(ush_pipeline_stdin_text);
            return 1;
        }

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

static void ush_grep_write_u64_dec(u64 value) {
    char tmp[32];
    u64 len = 0ULL;

    if (value == 0ULL) {
        ush_write_char('0');
        return;
    }

    while (value > 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0ULL) {
        len--;
        ush_write_char(tmp[len]);
    }
}

static int ush_grep_line_has_pattern(const char *line, u64 line_len, const char *pattern, u64 pattern_len) {
    u64 i;

    if (line == (const char *)0 || pattern == (const char *)0) {
        return 0;
    }

    if (pattern_len == 0ULL) {
        return 1;
    }

    if (pattern_len > line_len) {
        return 0;
    }

    for (i = 0ULL; i + pattern_len <= line_len; i++) {
        u64 j = 0ULL;

        while (j < pattern_len && line[i + j] == pattern[j]) {
            j++;
        }

        if (j == pattern_len) {
            return 1;
        }
    }

    return 0;
}

static u64 ush_grep_emit_matches(const char *input, u64 input_len, const char *pattern, int with_line_number) {
    u64 matches = 0ULL;
    u64 line_no = 1ULL;
    u64 start = 0ULL;
    u64 i;
    u64 pattern_len;

    if (input == (const char *)0 || pattern == (const char *)0) {
        return 0ULL;
    }

    pattern_len = ush_strlen(pattern);

    for (i = 0ULL; i <= input_len; i++) {
        if (i == input_len || input[i] == '\n') {
            u64 line_len = i - start;

            if (ush_grep_line_has_pattern(&input[start], line_len, pattern, pattern_len) != 0) {
                u64 j;

                matches++;

                if (with_line_number != 0) {
                    ush_grep_write_u64_dec(line_no);
                    ush_write(":");
                }

                for (j = 0ULL; j < line_len; j++) {
                    ush_write_char(input[start + j]);
                }

                ush_write_char('\n');
            }

            start = i + 1ULL;
            line_no++;
        }
    }

    return matches;
}

static int ush_cmd_grep(const ush_state *sh, const char *arg) {
    char first[USH_PATH_MAX];
    char second[USH_PATH_MAX];
    char third[USH_PATH_MAX];
    char path[USH_PATH_MAX];
    const char *rest = "";
    const char *rest2 = "";
    const char *pattern = (const char *)0;
    const char *file_arg = (const char *)0;
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    u64 size;
    u64 got;
    int with_line_number = 0;
    static char file_buf[USH_COPY_MAX + 1U];

    if (sh == (const ush_state *)0 || arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("grep: usage grep [-n] <pattern> [file]");
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        ush_writeln("grep: usage grep [-n] <pattern> [file]");
        return 0;
    }

    if (ush_streq(first, "-n") != 0) {
        with_line_number = 1;

        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            ush_writeln("grep: usage grep [-n] <pattern> [file]");
            return 0;
        }

        pattern = second;
        rest = rest2;
    } else {
        pattern = first;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, third, (u64)sizeof(third), &rest2) == 0) {
            ush_writeln("grep: usage grep [-n] <pattern> [file]");
            return 0;
        }

        file_arg = third;

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            ush_writeln("grep: usage grep [-n] <pattern> [file]");
            return 0;
        }
    }

    if (pattern == (const char *)0 || pattern[0] == '\0') {
        ush_writeln("grep: pattern required");
        return 0;
    }

    if (file_arg != (const char *)0) {
        if (ush_resolve_path(sh, file_arg, path, (u64)sizeof(path)) == 0) {
            ush_writeln("grep: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            ush_writeln("grep: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            ush_writeln("grep: failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            ush_writeln("grep: file too large for user buffer");
            return 0;
        }

        if (size == 0ULL) {
            return 1;
        }

        got = cleonos_sys_fs_read(path, file_buf, size);

        if (got == 0ULL || got != size) {
            ush_writeln("grep: read failed");
            return 0;
        }

        file_buf[got] = '\0';
        input = file_buf;
        input_len = got;
    } else {
        if (ush_pipeline_stdin_text == (const char *)0) {
            ush_writeln("grep: file path required (or pipeline input)");
            return 0;
        }

        input = ush_pipeline_stdin_text;
        input_len = ush_pipeline_stdin_len;
    }

    (void)ush_grep_emit_matches(input, input_len, pattern, with_line_number);
    return 1;
}

static void ush_text_error(const char *cmd, const char *message) {
    ush_write(cmd);
    ush_write(": ");
    ush_writeln(message);
}

static int ush_text_parse_optional_file(const char *arg, char *out_file, u64 out_file_size) {
    char first[USH_PATH_MAX];
    const char *rest = "";

    if (out_file == (char *)0 || out_file_size == 0ULL) {
        return 0;
    }

    out_file[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    ush_copy(out_file, out_file_size, first);
    return 1;
}

static int ush_text_load_input(const ush_state *sh,
                               const char *cmd,
                               const char *file_arg,
                               const char **out_input,
                               u64 *out_input_len) {
    static char file_buf[USH_COPY_MAX + 1U];
    char path[USH_PATH_MAX];
    u64 size;
    u64 got;

    if (sh == (const ush_state *)0 ||
        cmd == (const char *)0 ||
        out_input == (const char **)0 ||
        out_input_len == (u64 *)0) {
        return 0;
    }

    *out_input = (const char *)0;
    *out_input_len = 0ULL;

    if (file_arg != (const char *)0 && file_arg[0] != '\0') {
        if (ush_resolve_path(sh, file_arg, path, (u64)sizeof(path)) == 0) {
            ush_text_error(cmd, "invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            ush_text_error(cmd, "file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            ush_text_error(cmd, "failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            ush_text_error(cmd, "file too large for user buffer");
            return 0;
        }

        if (size == 0ULL) {
            file_buf[0] = '\0';
            *out_input = file_buf;
            *out_input_len = 0ULL;
            return 1;
        }

        got = cleonos_sys_fs_read(path, file_buf, size);

        if (got == 0ULL || got != size) {
            ush_text_error(cmd, "read failed");
            return 0;
        }

        file_buf[got] = '\0';
        *out_input = file_buf;
        *out_input_len = got;
        return 1;
    }

    if (ush_pipeline_stdin_text == (const char *)0) {
        ush_text_error(cmd, "file path required (or pipeline input)");
        return 0;
    }

    *out_input = ush_pipeline_stdin_text;
    *out_input_len = ush_pipeline_stdin_len;
    return 1;
}

static int ush_head_parse_args(const char *arg, u64 *out_line_count, char *out_file, u64 out_file_size) {
    char first[USH_PATH_MAX];
    char second[USH_PATH_MAX];
    char third[USH_PATH_MAX];
    const char *rest = "";
    const char *rest2 = "";

    if (out_line_count == (u64 *)0 || out_file == (char *)0 || out_file_size == 0ULL) {
        return 0;
    }

    *out_line_count = 10ULL;
    out_file[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "-n") != 0) {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            return 0;
        }

        if (ush_parse_u64_dec(second, out_line_count) == 0) {
            return 0;
        }

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            if (ush_split_first_and_rest(rest2, third, (u64)sizeof(third), &rest) == 0) {
                return 0;
            }

            if (rest != (const char *)0 && rest[0] != '\0') {
                return 0;
            }

            ush_copy(out_file, out_file_size, third);
        }

        return 1;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    ush_copy(out_file, out_file_size, first);
    return 1;
}

static int ush_cmd_head(const ush_state *sh, const char *arg) {
    char file_arg[USH_PATH_MAX];
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    u64 line_count;
    u64 i;
    u64 emitted = 0ULL;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_head_parse_args(arg, &line_count, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("head: usage head [-n N] [file]");
        return 0;
    }

    if (line_count == 0ULL) {
        return 1;
    }

    if (ush_text_load_input(sh, "head", file_arg, &input, &input_len) == 0) {
        return 0;
    }

    for (i = 0ULL; i < input_len; i++) {
        if (emitted >= line_count) {
            break;
        }

        ush_write_char(input[i]);

        if (input[i] == '\n') {
            emitted++;
        }
    }

    return 1;
}

static int ush_tail_parse_args(const char *arg, u64 *out_line_count, char *out_file, u64 out_file_size) {
    char first[USH_PATH_MAX];
    char second[USH_PATH_MAX];
    char third[USH_PATH_MAX];
    const char *rest = "";
    const char *rest2 = "";

    if (out_line_count == (u64 *)0 || out_file == (char *)0 || out_file_size == 0ULL) {
        return 0;
    }

    *out_line_count = 10ULL;
    out_file[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "-n") != 0) {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            return 0;
        }

        if (ush_parse_u64_dec(second, out_line_count) == 0) {
            return 0;
        }

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            if (ush_split_first_and_rest(rest2, third, (u64)sizeof(third), &rest) == 0) {
                return 0;
            }

            if (rest != (const char *)0 && rest[0] != '\0') {
                return 0;
            }

            ush_copy(out_file, out_file_size, third);
        }

        return 1;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    ush_copy(out_file, out_file_size, first);
    return 1;
}

static u64 ush_tail_count_lines(const char *input, u64 input_len) {
    u64 i;
    u64 lines = 0ULL;

    if (input == (const char *)0 || input_len == 0ULL) {
        return 0ULL;
    }

    lines = 1ULL;

    for (i = 0ULL; i < input_len; i++) {
        if (input[i] == '\n' && i + 1ULL < input_len) {
            lines++;
        }
    }

    return lines;
}

static u64 ush_tail_find_start_offset(const char *input, u64 input_len, u64 skip_lines) {
    u64 i;
    u64 lines_seen = 0ULL;

    if (input == (const char *)0 || input_len == 0ULL || skip_lines == 0ULL) {
        return 0ULL;
    }

    for (i = 0ULL; i < input_len; i++) {
        if (input[i] == '\n' && i + 1ULL < input_len) {
            lines_seen++;
            if (lines_seen == skip_lines) {
                return i + 1ULL;
            }
        }
    }

    return input_len;
}

static int ush_cmd_tail(const ush_state *sh, const char *arg) {
    char file_arg[USH_PATH_MAX];
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    u64 line_count;
    u64 total_lines;
    u64 skip_lines;
    u64 start_offset;
    u64 i;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_tail_parse_args(arg, &line_count, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("tail: usage tail [-n N] [file]");
        return 0;
    }

    if (line_count == 0ULL) {
        return 1;
    }

    if (ush_text_load_input(sh, "tail", file_arg, &input, &input_len) == 0) {
        return 0;
    }

    if (input_len == 0ULL) {
        return 1;
    }

    total_lines = ush_tail_count_lines(input, input_len);
    skip_lines = (total_lines > line_count) ? (total_lines - line_count) : 0ULL;
    start_offset = ush_tail_find_start_offset(input, input_len, skip_lines);

    for (i = start_offset; i < input_len; i++) {
        ush_write_char(input[i]);
    }

    return 1;
}

static int ush_cmd_wc(const ush_state *sh, const char *arg) {
    char file_arg[USH_PATH_MAX];
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    u64 bytes = 0ULL;
    u64 lines = 0ULL;
    u64 words = 0ULL;
    u64 i;
    int in_word = 0;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_text_parse_optional_file(arg, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("wc: usage wc [file]");
        return 0;
    }

    if (ush_text_load_input(sh, "wc", file_arg, &input, &input_len) == 0) {
        return 0;
    }

    bytes = input_len;

    for (i = 0ULL; i < input_len; i++) {
        char ch = input[i];

        if (ch == '\n') {
            lines++;
        }

        if (ush_is_space(ch) != 0) {
            in_word = 0;
        } else if (in_word == 0) {
            words++;
            in_word = 1;
        }
    }

    ush_grep_write_u64_dec(lines);
    ush_write(" ");
    ush_grep_write_u64_dec(words);
    ush_write(" ");
    ush_grep_write_u64_dec(bytes);
    ush_write_char('\n');

    return 1;
}

static int ush_cut_parse_delim(const char *token, char *out_delim) {
    if (token == (const char *)0 || out_delim == (char *)0) {
        return 0;
    }

    if (token[0] == '\\' && token[1] == 't' && token[2] == '\0') {
        *out_delim = '\t';
        return 1;
    }

    if (token[0] == '\0' || token[1] != '\0') {
        return 0;
    }

    *out_delim = token[0];
    return 1;
}

static int ush_cut_parse_args(const char *arg,
                              char *out_delim,
                              u64 *out_field,
                              char *out_file,
                              u64 out_file_size) {
    char token[USH_PATH_MAX];
    char value[USH_PATH_MAX];
    const char *cursor = arg;
    const char *rest = "";
    int delim_set = 0;
    int field_set = 0;
    int file_set = 0;
    u64 parsed_field = 0ULL;

    if (out_delim == (char *)0 || out_field == (u64 *)0 || out_file == (char *)0 || out_file_size == 0ULL) {
        return 0;
    }

    *out_delim = ',';
    *out_field = 1ULL;
    out_file[0] = '\0';

    if (cursor == (const char *)0 || cursor[0] == '\0') {
        return 0;
    }

    while (cursor != (const char *)0 && cursor[0] != '\0') {
        if (ush_split_first_and_rest(cursor, token, (u64)sizeof(token), &rest) == 0) {
            return 0;
        }

        if (ush_streq(token, "-d") != 0) {
            if (ush_split_first_and_rest(rest, value, (u64)sizeof(value), &cursor) == 0) {
                return 0;
            }

            if (ush_cut_parse_delim(value, out_delim) == 0) {
                return 0;
            }

            delim_set = 1;
            continue;
        }

        if (ush_streq(token, "-f") != 0) {
            if (ush_split_first_and_rest(rest, value, (u64)sizeof(value), &cursor) == 0) {
                return 0;
            }

            if (ush_parse_u64_dec(value, &parsed_field) == 0 || parsed_field == 0ULL) {
                return 0;
            }

            *out_field = parsed_field;
            field_set = 1;
            continue;
        }

        if (token[0] == '-') {
            return 0;
        }

        if (file_set != 0) {
            return 0;
        }

        ush_copy(out_file, out_file_size, token);
        file_set = 1;
        cursor = rest;
    }

    return (delim_set != 0 && field_set != 0) ? 1 : 0;
}

static void ush_cut_emit_field(const char *line, u64 line_len, char delim, u64 field_index) {
    u64 pos;
    u64 current_field = 1ULL;
    u64 field_start = 0ULL;

    for (pos = 0ULL; pos <= line_len; pos++) {
        if (pos == line_len || line[pos] == delim) {
            if (current_field == field_index) {
                u64 j;
                for (j = field_start; j < pos; j++) {
                    ush_write_char(line[j]);
                }
                break;
            }

            current_field++;
            field_start = pos + 1ULL;
        }
    }

    ush_write_char('\n');
}

static int ush_cmd_cut(const ush_state *sh, const char *arg) {
    char file_arg[USH_PATH_MAX];
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    char delim = ',';
    u64 field_index = 1ULL;
    u64 i;
    u64 start = 0ULL;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_cut_parse_args(arg, &delim, &field_index, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("cut: usage cut -d <char> -f <N> [file]");
        return 0;
    }

    if (ush_text_load_input(sh, "cut", file_arg, &input, &input_len) == 0) {
        return 0;
    }

    for (i = 0ULL; i < input_len; i++) {
        if (input[i] == '\n') {
            ush_cut_emit_field(&input[start], i - start, delim, field_index);
            start = i + 1ULL;
        }
    }

    if (start < input_len) {
        ush_cut_emit_field(&input[start], input_len - start, delim, field_index);
    }

    return 1;
}

static int ush_uniq_line_equal(const char *left, u64 left_len, const char *right, u64 right_len) {
    u64 i;

    if (left_len != right_len) {
        return 0;
    }

    for (i = 0ULL; i < left_len; i++) {
        if (left[i] != right[i]) {
            return 0;
        }
    }

    return 1;
}

static void ush_uniq_emit_line(const char *line, u64 line_len) {
    u64 i;

    for (i = 0ULL; i < line_len; i++) {
        ush_write_char(line[i]);
    }

    ush_write_char('\n');
}

static int ush_cmd_uniq(const ush_state *sh, const char *arg) {
    char file_arg[USH_PATH_MAX];
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    const char *prev_line = (const char *)0;
    u64 prev_len = 0ULL;
    u64 start = 0ULL;
    u64 i;
    int first = 1;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_text_parse_optional_file(arg, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("uniq: usage uniq [file]");
        return 0;
    }

    if (ush_text_load_input(sh, "uniq", file_arg, &input, &input_len) == 0) {
        return 0;
    }

    for (i = 0ULL; i < input_len; i++) {
        if (input[i] == '\n') {
            u64 line_len = i - start;
            const char *line = &input[start];

            if (first != 0 || ush_uniq_line_equal(prev_line, prev_len, line, line_len) == 0) {
                ush_uniq_emit_line(line, line_len);
                prev_line = line;
                prev_len = line_len;
                first = 0;
            }

            start = i + 1ULL;
        }
    }

    if (start < input_len) {
        u64 line_len = input_len - start;
        const char *line = &input[start];

        if (first != 0 || ush_uniq_line_equal(prev_line, prev_len, line, line_len) == 0) {
            ush_uniq_emit_line(line, line_len);
        }
    }

    return 1;
}

static int ush_sort_compare_line(const char *left, const char *right) {
    u64 i = 0ULL;

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] < right[i]) {
            return -1;
        }

        if (left[i] > right[i]) {
            return 1;
        }

        i++;
    }

    if (left[i] == right[i]) {
        return 0;
    }

    return (left[i] == '\0') ? -1 : 1;
}

static int ush_sort_collect_lines(char *buf, u64 len, char **out_lines, u64 max_lines, u64 *out_count) {
    u64 start = 0ULL;
    u64 i;
    u64 count = 0ULL;
    int truncated = 0;

    if (buf == (char *)0 || out_lines == (char **)0 || out_count == (u64 *)0 || max_lines == 0ULL) {
        return 0;
    }

    *out_count = 0ULL;

    for (i = 0ULL; i < len; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';

            if (count < max_lines) {
                out_lines[count++] = &buf[start];
            } else {
                truncated = 1;
            }

            start = i + 1ULL;
        }
    }

    if (start < len) {
        if (count < max_lines) {
            out_lines[count++] = &buf[start];
        } else {
            truncated = 1;
        }
    }

    *out_count = count;
    return (truncated == 0) ? 1 : 2;
}

static void ush_sort_lines(char **lines, u64 count) {
    u64 i;

    if (lines == (char **)0 || count == 0ULL) {
        return;
    }

    for (i = 1ULL; i < count; i++) {
        char *key = lines[i];
        u64 j = i;

        while (j > 0ULL && ush_sort_compare_line(lines[j - 1ULL], key) > 0) {
            lines[j] = lines[j - 1ULL];
            j--;
        }

        lines[j] = key;
    }
}

static int ush_cmd_sort(const ush_state *sh, const char *arg) {
    char file_arg[USH_PATH_MAX];
    const char *input = (const char *)0;
    u64 input_len = 0ULL;
    static char sort_buf[USH_COPY_MAX + 1U];
    char *lines[USH_SORT_MAX_LINES];
    u64 line_count = 0ULL;
    u64 i;
    int collect_status;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_text_parse_optional_file(arg, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("sort: usage sort [file]");
        return 0;
    }

    if (ush_text_load_input(sh, "sort", file_arg, &input, &input_len) == 0) {
        return 0;
    }

    for (i = 0ULL; i < input_len; i++) {
        sort_buf[i] = input[i];
    }
    sort_buf[input_len] = '\0';

    collect_status = ush_sort_collect_lines(sort_buf, input_len, lines, (u64)USH_SORT_MAX_LINES, &line_count);

    if (collect_status == 0) {
        ush_writeln("sort: internal parse error");
        return 0;
    }

    ush_sort_lines(lines, line_count);

    for (i = 0ULL; i < line_count; i++) {
        ush_writeln(lines[i]);
    }

    if (collect_status == 2) {
        ush_writeln("[sort] line count truncated");
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

static int ush_cmd_pid(void) {
    ush_print_kv_hex("PID", cleonos_sys_getpid());
    return 1;
}

static int ush_cmd_spawn(const ush_state *sh, const char *arg) {
    char target[USH_PATH_MAX];
    char argv_line[USH_ARG_MAX];
    char env_line[USH_PATH_MAX + 32ULL];
    const char *rest = "";
    char path[USH_PATH_MAX];
    u64 pid;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("spawn: usage spawn <path|name> [args...]");
        return 0;
    }

    if (ush_split_first_and_rest(arg, target, (u64)sizeof(target), &rest) == 0) {
        ush_writeln("spawn: usage spawn <path|name> [args...]");
        return 0;
    }

    argv_line[0] = '\0';
    if (rest != (const char *)0 && rest[0] != '\0') {
        ush_copy(argv_line, (u64)sizeof(argv_line), rest);
    }

    if (ush_resolve_exec_path(sh, target, path, (u64)sizeof(path)) == 0) {
        ush_writeln("spawn: invalid target");
        return 0;
    }

    if (ush_path_is_under_system(path) != 0) {
        ush_writeln("spawn: /system/*.elf is kernel-mode (KELF), not user-exec");
        return 0;
    }

    env_line[0] = '\0';
    ush_copy(env_line, (u64)sizeof(env_line), "PWD=");
    ush_copy(env_line + 4, (u64)(sizeof(env_line) - 4ULL), sh->cwd);

    pid = cleonos_sys_spawn_pathv(path, argv_line, env_line);

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
    if ((status & (1ULL << 63)) != 0ULL) {
        ush_print_kv_hex("  SIGNAL", status & 0xFFULL);
        ush_print_kv_hex("  VECTOR", (status >> 8) & 0xFFULL);
        ush_print_kv_hex("  ERROR", (status >> 16) & 0xFFFFULL);
    } else {
        ush_print_kv_hex("  STATUS", status);
    }
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

static int ush_cmd_shutdown(void) {
    ush_writeln("shutdown: powering off...");
    (void)cleonos_sys_shutdown();
    return 1;
}

static int ush_cmd_restart(void) {
    ush_writeln("restart: rebooting...");
    (void)cleonos_sys_restart();
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
    ush_write("\x1B[2J\x1B[H");
    return 1;
}

static int ush_cmd_ansi(void) {
    ush_writeln("\x1B[1;36mansi color demo\x1B[0m");
    ush_writeln("  \x1B[30mblack\x1B[0m \x1B[31mred\x1B[0m \x1B[32mgreen\x1B[0m \x1B[33myellow\x1B[0m");
    ush_writeln("  \x1B[34mblue\x1B[0m \x1B[35mmagenta\x1B[0m \x1B[36mcyan\x1B[0m \x1B[37mwhite\x1B[0m");
    ush_writeln("  \x1B[90mbright-black\x1B[0m \x1B[91mbright-red\x1B[0m \x1B[92mbright-green\x1B[0m \x1B[93mbright-yellow\x1B[0m");
    ush_writeln("  \x1B[94mbright-blue\x1B[0m \x1B[95mbright-magenta\x1B[0m \x1B[96mbright-cyan\x1B[0m \x1B[97mbright-white\x1B[0m");
    return 1;
}

static u64 ush_ansitest_u64_to_dec(char *out, u64 out_size, u64 value) {
    char rev[10];
    u64 digits = 0ULL;
    u64 i;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0ULL;
    }

    if (value == 0U) {
        if (out_size < 2ULL) {
            return 0ULL;
        }

        out[0] = '0';
        out[1] = '\0';
        return 1ULL;
    }

    while (value > 0U && digits < (u64)sizeof(rev)) {
        rev[digits++] = (char)('0' + (value % 10U));
        value /= 10U;
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

static void ush_ansitest_emit_bg256(u64 index) {
    char num[4];
    char seq[24];
    u64 digits;
    u64 p = 0ULL;
    u64 i;

    if (index > 255U) {
        index = 255U;
    }

    digits = ush_ansitest_u64_to_dec(num, (u64)sizeof(num), index);
    if (digits == 0ULL) {
        return;
    }

    seq[p++] = '\x1B';
    seq[p++] = '[';
    seq[p++] = '4';
    seq[p++] = '8';
    seq[p++] = ';';
    seq[p++] = '5';
    seq[p++] = ';';

    for (i = 0ULL; i < digits && p + 1ULL < (u64)sizeof(seq); i++) {
        seq[p++] = num[i];
    }

    if (p + 7ULL >= (u64)sizeof(seq)) {
        return;
    }

    seq[p++] = 'm';
    seq[p++] = ' ';
    seq[p++] = ' ';
    seq[p++] = '\x1B';
    seq[p++] = '[';
    seq[p++] = '0';
    seq[p++] = 'm';
    seq[p] = '\0';

    ush_write(seq);
}

static int ush_cmd_ansitest(void) {
    u64 i;

    ush_writeln("\x1B[1;96mANSI test suite\x1B[0m");
    ush_writeln("styles: \x1B[1mbold\x1B[0m  \x1B[7minverse\x1B[0m  \x1B[4munderline\x1B[0m");
    ush_writeln("16-color demo:");
    (void)ush_cmd_ansi();

    ush_writeln("256-color palette (0..255):");
    for (i = 0ULL; i < 256ULL; i++) {
        ush_ansitest_emit_bg256(i);
        if ((i % 32ULL) == 31ULL) {
            ush_write_char('\n');
        }
    }
    ush_write_char('\n');

    ush_writeln("truecolor demo:");
    ush_writeln("  \x1B[38;2;255;64;64mRGB(255,64,64)\x1B[0m  \x1B[38;2;64;255;64mRGB(64,255,64)\x1B[0m  \x1B[38;2;64;128;255mRGB(64,128,255)\x1B[0m");

    ush_writeln("cursor control demo:");
    ush_write("  0123456789");
    ush_write("\x1B[5D");
    ush_write("\x1B[93m<OK>\x1B[0m");
    ush_write_char('\n');

    ush_write("  save");
    ush_write("\x1B[s");
    ush_write("....");
    ush_write("\x1B[u");
    ush_write("\x1B[92m<restore>\x1B[0m");
    ush_write_char('\n');

    ush_writeln("erase-line demo:");
    ush_write("  left|right-to-clear");
    ush_write("\x1B[14D\x1B[K");
    ush_write_char('\n');

    ush_writeln("ansitest done");
    return 1;
}

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
    const char *payload = (const char *)0;
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

    if (payload == (const char *)0 || payload[0] == '\0') {
        if (ush_pipeline_stdin_text == (const char *)0) {
            ush_writeln("write: usage write <file> <text>");
            return 0;
        }
        payload = ush_pipeline_stdin_text;
        payload_len = ush_pipeline_stdin_len;
    } else {
        payload_len = ush_strlen(payload);
    }

    if (cleonos_sys_fs_write(abs_path, payload, payload_len) == 0ULL) {
        ush_writeln("write: failed");
        return 0;
    }

    return 1;
}

static int ush_cmd_append(const ush_state *sh, const char *arg) {
    char path_arg[USH_PATH_MAX];
    char abs_path[USH_PATH_MAX];
    const char *payload = (const char *)0;
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

    if (payload == (const char *)0 || payload[0] == '\0') {
        if (ush_pipeline_stdin_text == (const char *)0) {
            ush_writeln("append: usage append <file> <text>");
            return 0;
        }
        payload = ush_pipeline_stdin_text;
        payload_len = ush_pipeline_stdin_len;
    } else {
        payload_len = ush_strlen(payload);
    }

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

static int ush_execute_single_command(ush_state *sh,
                                      const char *cmd,
                                      const char *arg,
                                      int allow_external,
                                      int *out_known,
                                      int *out_success) {
    int known = 1;
    int success = 0;

    if (out_known != (int *)0) {
        *out_known = 1;
    }

    if (out_success != (int *)0) {
        *out_success = 0;
    }

    if (sh == (ush_state *)0 || cmd == (const char *)0 || cmd[0] == '\0') {
        if (out_known != (int *)0) {
            *out_known = 0;
        }
        return 0;
    }

    if (allow_external != 0 && ush_try_exec_external(sh, cmd, arg, &success) != 0) {
        if (out_success != (int *)0) {
            *out_success = success;
        }
        return 1;
    }

    if (ush_streq(cmd, "help") != 0) {
        success = ush_cmd_help();
    } else if (ush_streq(cmd, "ls") != 0 || ush_streq(cmd, "dir") != 0) {
        success = ush_cmd_ls(sh, arg);
    } else if (ush_streq(cmd, "cat") != 0) {
        success = ush_cmd_cat(sh, arg);
    } else if (ush_streq(cmd, "grep") != 0) {
        success = ush_cmd_grep(sh, arg);
    } else if (ush_streq(cmd, "head") != 0) {
        success = ush_cmd_head(sh, arg);
    } else if (ush_streq(cmd, "tail") != 0) {
        success = ush_cmd_tail(sh, arg);
    } else if (ush_streq(cmd, "wc") != 0) {
        success = ush_cmd_wc(sh, arg);
    } else if (ush_streq(cmd, "cut") != 0) {
        success = ush_cmd_cut(sh, arg);
    } else if (ush_streq(cmd, "uniq") != 0) {
        success = ush_cmd_uniq(sh, arg);
    } else if (ush_streq(cmd, "sort") != 0) {
        success = ush_cmd_sort(sh, arg);
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
    } else if (ush_streq(cmd, "shutdown") != 0 || ush_streq(cmd, "poweroff") != 0) {
        success = ush_cmd_shutdown();
    } else if (ush_streq(cmd, "restart") != 0 || ush_streq(cmd, "reboot") != 0) {
        success = ush_cmd_restart();
    } else if (ush_streq(cmd, "exit") != 0) {
        success = ush_cmd_exit(sh, arg);
    } else if (ush_streq(cmd, "clear") != 0 || ush_streq(cmd, "cls") != 0) {
        success = ush_cmd_clear();
    } else if (ush_streq(cmd, "ansi") != 0 || ush_streq(cmd, "color") != 0) {
        success = ush_cmd_ansi();
    } else if (ush_streq(cmd, "ansitest") != 0) {
        success = ush_cmd_ansitest();
    } else if (ush_streq(cmd, "fastfetch") != 0) {
        success = ush_cmd_fastfetch(arg);
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

    if (out_known != (int *)0) {
        *out_known = known;
    }

    if (out_success != (int *)0) {
        *out_success = success;
    }

    return 1;
}

static void ush_pipeline_set_stdin(const char *text, u64 len) {
    ush_pipeline_stdin_text = text;
    ush_pipeline_stdin_len = len;
}

static int ush_pipeline_has_meta(const char *line) {
    u64 i = 0ULL;

    if (line == (const char *)0) {
        return 0;
    }

    while (line[i] != '\0') {
        if (line[i] == '|' || line[i] == '>') {
            return 1;
        }
        i++;
    }

    return 0;
}

static int ush_pipeline_parse_stage(ush_pipeline_stage *stage, const char *segment_text) {
    char work[USH_LINE_MAX];
    char path_part[USH_PATH_MAX];
    const char *path_rest = "";
    u64 i;
    i64 op_pos = -1;
    int op_mode = 0;

    if (stage == (ush_pipeline_stage *)0 || segment_text == (const char *)0) {
        return 0;
    }

    ush_copy(work, (u64)sizeof(work), segment_text);
    ush_trim_line(work);

    if (work[0] == '\0') {
        return 0;
    }

    for (i = 0ULL; work[i] != '\0'; i++) {
        if (work[i] == '>') {
            if (op_pos >= 0) {
                ush_writeln("pipe: multiple redirections in one stage are not supported");
                return 0;
            }

            op_pos = (i64)i;

            if (work[i + 1ULL] == '>') {
                op_mode = 2;
                i++;
            } else {
                op_mode = 1;
            }
        }
    }

    stage->redirect_mode = 0;
    stage->redirect_path[0] = '\0';

    if (op_pos >= 0) {
        char *path_src;

        work[(u64)op_pos] = '\0';
        path_src = &work[(u64)op_pos + ((op_mode == 2) ? 2ULL : 1ULL)];

        ush_trim_line(work);
        ush_trim_line(path_src);

        if (path_src[0] == '\0') {
            ush_writeln("pipe: redirection path required");
            return 0;
        }

        if (ush_split_first_and_rest(path_src, path_part, (u64)sizeof(path_part), &path_rest) == 0) {
            ush_writeln("pipe: redirection path required");
            return 0;
        }

        if (path_rest != (const char *)0 && path_rest[0] != '\0') {
            ush_writeln("pipe: redirection path cannot contain spaces");
            return 0;
        }

        stage->redirect_mode = op_mode;
        ush_copy(stage->redirect_path, (u64)sizeof(stage->redirect_path), path_part);
    }

    ush_copy(stage->text, (u64)sizeof(stage->text), work);
    ush_parse_line(work, stage->cmd, (u64)sizeof(stage->cmd), stage->arg, (u64)sizeof(stage->arg));
    ush_trim_line(stage->arg);

    if (stage->cmd[0] == '\0') {
        ush_writeln("pipe: empty command stage");
        return 0;
    }

    return 1;
}

static int ush_pipeline_parse(const char *line,
                              ush_pipeline_stage *stages,
                              u64 max_stages,
                              u64 *out_stage_count) {
    char segment[USH_LINE_MAX];
    u64 i = 0ULL;
    u64 seg_pos = 0ULL;
    u64 stage_count = 0ULL;

    if (line == (const char *)0 || stages == (ush_pipeline_stage *)0 || max_stages == 0ULL || out_stage_count == (u64 *)0) {
        return 0;
    }

    *out_stage_count = 0ULL;

    for (;;) {
        char ch = line[i];

        if (ch == '|' || ch == '\0') {
            segment[seg_pos] = '\0';

            if (stage_count >= max_stages) {
                ush_writeln("pipe: too many stages");
                return 0;
            }

            if (ush_pipeline_parse_stage(&stages[stage_count], segment) == 0) {
                return 0;
            }

            stage_count++;
            seg_pos = 0ULL;

            if (ch == '\0') {
                break;
            }

            i++;
            continue;
        }

        if (seg_pos + 1ULL >= (u64)sizeof(segment)) {
            ush_writeln("pipe: stage text too long");
            return 0;
        }

        segment[seg_pos++] = ch;
        i++;
    }

    *out_stage_count = stage_count;
    return 1;
}

static int ush_pipeline_write_redirect(const ush_state *sh, const ush_pipeline_stage *stage, const char *data, u64 len) {
    char abs_path[USH_PATH_MAX];
    u64 ok;

    if (sh == (const ush_state *)0 || stage == (const ush_pipeline_stage *)0) {
        return 0;
    }

    if (stage->redirect_mode == 0) {
        return 1;
    }

    if (ush_resolve_path(sh, stage->redirect_path, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln("redirect: invalid path");
        return 0;
    }

    if (stage->redirect_mode == 1) {
        ok = cleonos_sys_fs_write(abs_path, data, len);
    } else {
        ok = cleonos_sys_fs_append(abs_path, data, len);
    }

    if (ok == 0ULL) {
        ush_writeln("redirect: write failed");
        return 0;
    }

    return 1;
}

static int ush_execute_pipeline(ush_state *sh,
                                const char *line,
                                int *out_known,
                                int *out_success) {
    ush_pipeline_stage stages[USH_PIPELINE_MAX_STAGES];
    u64 stage_count = 0ULL;
    u64 i;
    const char *pipe_in = (const char *)0;
    u64 pipe_in_len = 0ULL;
    char *capture_out = ush_pipeline_capture_a;
    u64 capture_len = 0ULL;
    int known = 1;
    int success = 1;

    if (out_known != (int *)0) {
        *out_known = 1;
    }

    if (out_success != (int *)0) {
        *out_success = 0;
    }

    if (ush_pipeline_parse(line, stages, USH_PIPELINE_MAX_STAGES, &stage_count) == 0) {
        return 0;
    }

    for (i = 0ULL; i < stage_count; i++) {
        int stage_known = 1;
        int stage_success = 0;
        int mirror_to_tty = ((i + 1ULL) == stage_count && stages[i].redirect_mode == 0) ? 1 : 0;

        if (i + 1ULL < stage_count && stages[i].redirect_mode != 0) {
            ush_writeln("pipe: redirection is only supported on final stage");
            known = 1;
            success = 0;
            break;
        }

        ush_pipeline_set_stdin(pipe_in, pipe_in_len);
        ush_output_capture_begin(capture_out, (u64)USH_PIPE_CAPTURE_MAX + 1ULL, mirror_to_tty);
        (void)ush_execute_single_command(sh, stages[i].cmd, stages[i].arg, 0, &stage_known, &stage_success);
        capture_len = ush_output_capture_end();

        if (ush_output_capture_truncated() != 0) {
            ush_writeln("[pipe] captured output truncated");
        }

        if (stage_known == 0) {
            known = 0;
        }

        if (stage_success == 0) {
            success = 0;
            break;
        }

        if (stages[i].redirect_mode != 0) {
            if (ush_pipeline_write_redirect(sh, &stages[i], capture_out, capture_len) == 0) {
                success = 0;
                break;
            }
        }

        pipe_in = capture_out;
        pipe_in_len = capture_len;
        capture_out = (capture_out == ush_pipeline_capture_a) ? ush_pipeline_capture_b : ush_pipeline_capture_a;
    }

    ush_pipeline_set_stdin((const char *)0, 0ULL);

    if (out_known != (int *)0) {
        *out_known = known;
    }

    if (out_success != (int *)0) {
        *out_success = success;
    }

    return 1;
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

    if (ush_pipeline_has_meta(line_buf) != 0) {
        if (ush_execute_pipeline(sh, line_buf, &known, &success) == 0) {
            known = 1;
            success = 0;
        }
    } else {
        ush_parse_line(line_buf, cmd, (u64)sizeof(cmd), arg, (u64)sizeof(arg));
        ush_trim_line(arg);
        (void)ush_execute_single_command(sh, cmd, arg, 1, &known, &success);
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



