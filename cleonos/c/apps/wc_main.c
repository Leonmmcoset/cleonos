#include "cmd_runtime.h"

static void ush_wc_write_u64_dec(u64 value) {
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

static int ush_wc_parse_args(const char *arg, char *out_file, u64 out_file_size) {
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

static int ush_wc_load_input(const ush_state *sh, const char *file_arg, const char **out_input, u64 *out_input_len) {
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
            ush_writeln("wc: invalid path");
            return 0;
        }

        if (cleonos_sys_fs_stat_type(path) != 1ULL) {
            ush_writeln("wc: file not found");
            return 0;
        }

        size = cleonos_sys_fs_stat_size(path);

        if (size == (u64)-1) {
            ush_writeln("wc: failed to stat file");
            return 0;
        }

        if (size > (u64)USH_COPY_MAX) {
            ush_writeln("wc: file too large for user buffer");
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
            ush_writeln("wc: read failed");
            return 0;
        }

        file_buf[got] = '\0';
        *out_input = file_buf;
        *out_input_len = got;
        return 1;
    }

    if (ush_pipeline_stdin_text == (const char *)0) {
        ush_writeln("wc: file path required (or pipeline input)");
        return 0;
    }

    *out_input = ush_pipeline_stdin_text;
    *out_input_len = ush_pipeline_stdin_len;
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

    if (ush_wc_parse_args(arg, file_arg, (u64)sizeof(file_arg)) == 0) {
        ush_writeln("wc: usage wc [file]");
        return 0;
    }

    if (ush_wc_load_input(sh, file_arg, &input, &input_len) == 0) {
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

    ush_wc_write_u64_dec(lines);
    ush_write(" ");
    ush_wc_write_u64_dec(words);
    ush_write(" ");
    ush_wc_write_u64_dec(bytes);
    ush_write_char('\n');

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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "wc") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_wc(&sh, arg);

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
