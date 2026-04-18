#include "cmd_runtime.h"
#include <stdio.h>

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

static int ush_tail_load_input(const ush_state *sh, const char *file_arg, const char **out_input, u64 *out_input_len) {
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
            (void)puts("tail: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            (void)puts("tail: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            (void)puts("tail: failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            (void)puts("tail: file too large for user buffer");
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
            (void)puts("tail: read failed");
            return 0;
        }

        file_buf[got] = '\0';
        *out_input = file_buf;
        *out_input_len = got;
        return 1;
    }

    if (ush_pipeline_stdin_text == (const char *)0) {
        (void)puts("tail: file path required (or pipeline input)");
        return 0;
    }

    *out_input = ush_pipeline_stdin_text;
    *out_input_len = ush_pipeline_stdin_len;
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
        (void)puts("tail: usage tail [-n N] [file]");
        return 0;
    }

    if (ush_tail_load_input(sh, file_arg, &input, &input_len) == 0) {
        return 0;
    }

    if (line_count == 0ULL || input_len == 0ULL) {
        return 1;
    }

    total_lines = ush_tail_count_lines(input, input_len);
    skip_lines = (total_lines > line_count) ? (total_lines - line_count) : 0ULL;
    start_offset = ush_tail_find_start_offset(input, input_len, skip_lines);

    for (i = start_offset; i < input_len; i++) {
        (void)putchar((unsigned char)input[i]);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "tail") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_tail(&sh, arg);

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
