#include "cmd_runtime.h"

#define USH_SORT_MAX_LINES 4096ULL

static int ush_sort_parse_args(const char *arg, char *out_file, u64 out_file_size) {
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

static int ush_sort_load_input(const ush_state *sh, const char *file_arg, char *out_buf, u64 out_buf_size, u64 *out_len) {
    char path[USH_PATH_MAX];
    u64 size;
    u64 got;
    u64 i;

    if (sh == (const ush_state *)0 || out_buf == (char *)0 || out_buf_size == 0ULL || out_len == (u64 *)0) {
        return 0;
    }

    *out_len = 0ULL;
    out_buf[0] = '\0';

    if (file_arg != (const char *)0 && file_arg[0] != '\0') {
        if (ush_resolve_path(sh, file_arg, path, (u64)sizeof(path)) == 0) {
            ush_writeln("sort: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            ush_writeln("sort: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            ush_writeln("sort: failed to stat file");
            return 0;
        }

        if (size + 1ULL > out_buf_size) {
            ush_writeln("sort: file too large for user buffer");
            return 0;
        }

        if (size == 0ULL) {
            return 1;
        }

        got = cleonos_sys_fs_read(path, out_buf, size);

        if (got == 0ULL || got != size) {
            ush_writeln("sort: read failed");
            return 0;
        }

        out_buf[got] = '\0';
        *out_len = got;
        return 1;
    }

    if (ush_pipeline_stdin_text == (const char *)0) {
        ush_writeln("sort: file path required (or pipeline input)");
        return 0;
    }

    if (ush_pipeline_stdin_len + 1ULL > out_buf_size) {
        ush_writeln("sort: pipeline input too large");
        return 0;
    }

    for (i = 0ULL; i < ush_pipeline_stdin_len; i++) {
        out_buf[i] = ush_pipeline_stdin_text[i];
    }

    out_buf[ush_pipeline_stdin_len] = '\0';
    *out_len = ush_pipeline_stdin_len;
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
    static char sort_buf[USH_COPY_MAX + 1U];
    char *lines[USH_SORT_MAX_LINES];
    u64 len = 0ULL;
    u64 line_count = 0ULL;
    u64 i;
    int collect_status;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_sort_parse_args(arg, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("sort: usage sort [file]");
        return 0;
    }

    if (ush_sort_load_input(sh, file_arg, sort_buf, (u64)sizeof(sort_buf), &len) == 0) {
        return 0;
    }

    collect_status = ush_sort_collect_lines(sort_buf, len, lines, (u64)USH_SORT_MAX_LINES, &line_count);

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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "sort") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_sort(&sh, arg);

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
