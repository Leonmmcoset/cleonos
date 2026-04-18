#include "cmd_runtime.h"

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

static int ush_cut_parse_args(const char *arg, char *out_delim, u64 *out_field, char *out_file, u64 out_file_size) {
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

static int ush_cut_load_input(const ush_state *sh, const char *file_arg, const char **out_input, u64 *out_input_len) {
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
            ush_writeln("cut: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            ush_writeln("cut: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            ush_writeln("cut: failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            ush_writeln("cut: file too large for user buffer");
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
            ush_writeln("cut: read failed");
            return 0;
        }

        file_buf[got] = '\0';
        *out_input = file_buf;
        *out_input_len = got;
        return 1;
    }

    if (ush_pipeline_stdin_text == (const char *)0) {
        ush_writeln("cut: file path required (or pipeline input)");
        return 0;
    }

    *out_input = ush_pipeline_stdin_text;
    *out_input_len = ush_pipeline_stdin_len;
    return 1;
}

static void ush_cut_emit_field(const char *line, u64 line_len, char delim, u64 field_index) {
    u64 pos;
    u64 current_field = 1ULL;
    u64 field_start = 0ULL;
    int emitted = 0;

    for (pos = 0ULL; pos <= line_len; pos++) {
        if (pos == line_len || line[pos] == delim) {
            if (current_field == field_index) {
                u64 j;
                for (j = field_start; j < pos; j++) {
                    ush_write_char(line[j]);
                }
                emitted = 1;
                break;
            }

            current_field++;
            field_start = pos + 1ULL;
        }
    }

    if (emitted == 0) {
        /* keep empty line */
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

    if (ush_cut_load_input(sh, file_arg, &input, &input_len) == 0) {
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "cut") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_cut(&sh, arg);

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
