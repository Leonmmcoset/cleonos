#include "cmd_runtime.h"
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


int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "ansitest") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_ansitest();

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

