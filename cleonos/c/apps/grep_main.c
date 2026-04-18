#include "cmd_runtime.h"
#include <stdio.h>

static void ush_grep_write_u64_dec(u64 value) {
    char tmp[32];
    u64 len = 0ULL;

    if (value == 0ULL) {
        (void)putchar('0');
        return;
    }

    while (value > 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0ULL) {
        len--;
        (void)putchar((unsigned char)tmp[len]);
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
                    (void)fputs(":", 1);
                }

                for (j = 0ULL; j < line_len; j++) {
                    (void)putchar((unsigned char)input[start + j]);
                }

                (void)putchar('\n');
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
        (void)puts("grep: usage grep [-n] <pattern> [file]");
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        (void)puts("grep: usage grep [-n] <pattern> [file]");
        return 0;
    }

    if (ush_streq(first, "-n") != 0) {
        with_line_number = 1;

        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            (void)puts("grep: usage grep [-n] <pattern> [file]");
            return 0;
        }

        pattern = second;
        rest = rest2;
    } else {
        pattern = first;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, third, (u64)sizeof(third), &rest2) == 0) {
            (void)puts("grep: usage grep [-n] <pattern> [file]");
            return 0;
        }

        file_arg = third;

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            (void)puts("grep: usage grep [-n] <pattern> [file]");
            return 0;
        }
    }

    if (pattern == (const char *)0 || pattern[0] == '\0') {
        (void)puts("grep: pattern required");
        return 0;
    }

    if (file_arg != (const char *)0) {
        if (ush_resolve_path(sh, file_arg, path, (u64)sizeof(path)) == 0) {
            (void)puts("grep: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            (void)puts("grep: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            (void)puts("grep: failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            (void)puts("grep: file too large for user buffer");
            return 0;
        }

        if (size == 0ULL) {
            return 1;
        }

        got = cleonos_sys_fs_read(path, file_buf, size);

        if (got == 0ULL || got != size) {
            (void)puts("grep: read failed");
            return 0;
        }

        file_buf[got] = '\0';
        input = file_buf;
        input_len = got;
    } else {
        if (ush_pipeline_stdin_text == (const char *)0) {
            (void)puts("grep: file path required (or pipeline input)");
            return 0;
        }

        input = ush_pipeline_stdin_text;
        input_len = ush_pipeline_stdin_len;
    }

    (void)ush_grep_emit_matches(input, input_len, pattern, with_line_number);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "grep") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_grep(&sh, arg);

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

