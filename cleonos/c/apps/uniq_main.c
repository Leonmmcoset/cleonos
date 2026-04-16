#include "cmd_runtime.h"

static int ush_uniq_parse_args(const char *arg, char *out_file, u64 out_file_size) {
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

static int ush_uniq_load_input(const ush_state *sh, const char *file_arg, const char **out_input, u64 *out_input_len) {
    static char file_buf[USH_COPY_MAX + 1U];
    char path[USH_PATH_MAX];
    u64 size;
    u64 got;

    if (sh == (const ush_state *)0 || out_input == (const char **)0 || out_input_len == (u64 *)0) {
        return 0;
    }

    *out_input = (const char *)0;
    *out_input_len = 0ULL;

    if (file_arg != (const char *)0 && file_arg[0] != '\0') {
        if (ush_resolve_path(sh, file_arg, path, (u64)sizeof(path)) == 0) {
            ush_writeln("uniq: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            ush_writeln("uniq: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            ush_writeln("uniq: failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            ush_writeln("uniq: file too large for user buffer");
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
            ush_writeln("uniq: read failed");
            return 0;
        }

        file_buf[got] = '\0';
        *out_input = file_buf;
        *out_input_len = got;
        return 1;
    }

    if (ush_pipeline_stdin_text == (const char *)0) {
        ush_writeln("uniq: file path required (or pipeline input)");
        return 0;
    }

    *out_input = ush_pipeline_stdin_text;
    *out_input_len = ush_pipeline_stdin_len;
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

    if (ush_uniq_parse_args(arg, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("uniq: usage uniq [file]");
        return 0;
    }

    if (ush_uniq_load_input(sh, file_arg, &input, &input_len) == 0) {
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "uniq") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_uniq(&sh, arg);

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
