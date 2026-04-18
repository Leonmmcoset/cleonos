#include "cmd_runtime.h"

#define USH_KDBG_TEXT_MAX 2048ULL

static int ush_kdbg_is_hex(char ch) {
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
        return 1;
    }

    return 0;
}

static u64 ush_kdbg_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return (u64)(ch - '0');
    }

    if (ch >= 'a' && ch <= 'f') {
        return 10ULL + (u64)(ch - 'a');
    }

    return 10ULL + (u64)(ch - 'A');
}

static int ush_kdbg_parse_u64(const char *text, u64 *out_value) {
    u64 value = 0ULL;
    u64 i = 0ULL;
    int is_hex = 0;

    if (text == (const char *)0 || out_value == (u64 *)0 || text[0] == '\0') {
        return 0;
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        i = 2ULL;
        is_hex = 1;
    } else if (text[0] == 'x' || text[0] == 'X') {
        i = 1ULL;
        is_hex = 1;
    }

    if (is_hex != 0) {
        if (text[i] == '\0') {
            return 0;
        }

        while (text[i] != '\0') {
            u64 digit;

            if (ush_kdbg_is_hex(text[i]) == 0) {
                return 0;
            }

            digit = ush_kdbg_hex_value(text[i]);

            if (value > ((0xFFFFFFFFFFFFFFFFULL - digit) >> 4U)) {
                return 0;
            }

            value = (value << 4U) | digit;
            i++;
        }

        *out_value = value;
        return 1;
    }

    return ush_parse_u64_dec(text, out_value);
}

static int ush_kdbg_next_token(const char **io_cursor, char *out, u64 out_size) {
    const char *p;
    u64 n = 0ULL;

    if (io_cursor == (const char **)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    p = *io_cursor;
    out[0] = '\0';

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

static int ush_kdbg_has_more_tokens(const char *cursor) {
    if (cursor == (const char *)0) {
        return 0;
    }

    while (*cursor != '\0') {
        if (ush_is_space(*cursor) == 0) {
            return 1;
        }

        cursor++;
    }

    return 0;
}

static void ush_kdbg_usage(void) {
    ush_writeln("usage:");
    ush_writeln("  kdbg sym <addr>");
    ush_writeln("  kdbg bt <rbp> <rip>");
    ush_writeln("  kdbg regs");
}

static int ush_cmd_kdbg(const char *arg) {
    const char *cursor = arg;
    char subcmd[USH_CMD_MAX];
    char tok0[USH_PATH_MAX];
    char tok1[USH_PATH_MAX];
    char out[USH_KDBG_TEXT_MAX];
    u64 got;
    u64 value0;
    u64 value1;

    if (ush_kdbg_next_token(&cursor, subcmd, (u64)sizeof(subcmd)) == 0) {
        ush_kdbg_usage();
        return 0;
    }

    if (ush_streq(subcmd, "sym") != 0) {
        if (ush_kdbg_next_token(&cursor, tok0, (u64)sizeof(tok0)) == 0 || ush_kdbg_has_more_tokens(cursor) != 0) {
            ush_kdbg_usage();
            return 0;
        }

        if (ush_kdbg_parse_u64(tok0, &value0) == 0) {
            ush_writeln("kdbg: invalid addr");
            return 0;
        }

        got = cleonos_sys_kdbg_sym(value0, out, (u64)sizeof(out));

        if (got == 0ULL) {
            ush_writeln("kdbg: sym failed");
            return 0;
        }

        out[(u64)sizeof(out) - 1ULL] = '\0';
        ush_writeln(out);
        return 1;
    }

    if (ush_streq(subcmd, "bt") != 0) {
        if (ush_kdbg_next_token(&cursor, tok0, (u64)sizeof(tok0)) == 0 ||
            ush_kdbg_next_token(&cursor, tok1, (u64)sizeof(tok1)) == 0 || ush_kdbg_has_more_tokens(cursor) != 0) {
            ush_kdbg_usage();
            return 0;
        }

        if (ush_kdbg_parse_u64(tok0, &value0) == 0 || ush_kdbg_parse_u64(tok1, &value1) == 0) {
            ush_writeln("kdbg: invalid rbp/rip");
            return 0;
        }

        got = cleonos_sys_kdbg_bt(value0, value1, out, (u64)sizeof(out));

        if (got == 0ULL) {
            ush_writeln("kdbg: bt failed");
            return 0;
        }

        out[(u64)sizeof(out) - 1ULL] = '\0';
        ush_write(out);
        if (got > (u64)sizeof(out) - 1ULL) {
            got = (u64)sizeof(out) - 1ULL;
        }
        if (got > 0ULL && out[got - 1ULL] != '\n') {
            ush_write_char('\n');
        }
        return 1;
    }

    if (ush_streq(subcmd, "regs") != 0) {
        if (ush_kdbg_has_more_tokens(cursor) != 0) {
            ush_kdbg_usage();
            return 0;
        }

        got = cleonos_sys_kdbg_regs(out, (u64)sizeof(out));

        if (got == 0ULL) {
            ush_writeln("kdbg: regs failed");
            return 0;
        }

        out[(u64)sizeof(out) - 1ULL] = '\0';
        ush_write(out);
        if (got > (u64)sizeof(out) - 1ULL) {
            got = (u64)sizeof(out) - 1ULL;
        }
        if (got > 0ULL && out[got - 1ULL] != '\n') {
            ush_write_char('\n');
        }
        return 1;
    }

    ush_kdbg_usage();
    return 0;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "kdbg") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_kdbg(arg);

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
