#include "shell_internal.h"

void ush_init_state(ush_state *sh) {
    if (sh == (ush_state *)0) {
        return;
    }

    sh->line[0] = '\0';
    sh->line_len = 0ULL;
    sh->cursor = 0ULL;
    sh->rendered_len = 0ULL;

    ush_copy(sh->cwd, (u64)sizeof(sh->cwd), "/");

    sh->history_count = 0ULL;
    sh->history_nav = -1;
    sh->nav_saved_line[0] = '\0';
    sh->nav_saved_len = 0ULL;
    sh->nav_saved_cursor = 0ULL;

    sh->cmd_total = 0ULL;
    sh->cmd_ok = 0ULL;
    sh->cmd_fail = 0ULL;
    sh->cmd_unknown = 0ULL;
    sh->exit_requested = 0;
    sh->exit_code = 0ULL;
}

u64 ush_strlen(const char *str) {
    u64 len = 0ULL;

    if (str == (const char *)0) {
        return 0ULL;
    }

    while (str[len] != '\0') {
        len++;
    }

    return len;
}

int ush_streq(const char *left, const char *right) {
    u64 i = 0ULL;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }

    return (left[i] == right[i]) ? 1 : 0;
}

int ush_is_space(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ? 1 : 0;
}

int ush_is_printable(char ch) {
    return (ch >= 32 && ch <= 126) ? 1 : 0;
}

int ush_has_suffix(const char *name, const char *suffix) {
    u64 name_len;
    u64 suffix_len;
    u64 i;

    if (name == (const char *)0 || suffix == (const char *)0) {
        return 0;
    }

    name_len = ush_strlen(name);
    suffix_len = ush_strlen(suffix);

    if (suffix_len > name_len) {
        return 0;
    }

    for (i = 0ULL; i < suffix_len; i++) {
        if (name[name_len - suffix_len + i] != suffix[i]) {
            return 0;
        }
    }

    return 1;
}

int ush_contains_char(const char *text, char needle) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] == needle) {
            return 1;
        }
        i++;
    }

    return 0;
}

int ush_parse_u64_dec(const char *text, u64 *out_value) {
    u64 value = 0ULL;
    u64 i = 0ULL;

    if (text == (const char *)0 || out_value == (u64 *)0 || text[0] == '\0') {
        return 0;
    }

    while (text[i] != '\0') {
        u64 digit;

        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }

        digit = (u64)(text[i] - '0');

        if (value > ((0xFFFFFFFFFFFFFFFFULL - digit) / 10ULL)) {
            return 0;
        }

        value = (value * 10ULL) + digit;
        i++;
    }

    *out_value = value;
    return 1;
}

void ush_copy(char *dst, u64 dst_size, const char *src) {
    u64 i = 0ULL;

    if (dst == (char *)0 || src == (const char *)0 || dst_size == 0ULL) {
        return;
    }

    while (src[i] != '\0' && i + 1ULL < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

void ush_trim_line(char *line) {
    u64 start = 0ULL;
    u64 i = 0ULL;
    u64 len;

    if (line == (char *)0) {
        return;
    }

    while (line[start] != '\0' && ush_is_space(line[start]) != 0) {
        start++;
    }

    if (start > 0ULL) {
        while (line[start + i] != '\0') {
            line[i] = line[start + i];
            i++;
        }
        line[i] = '\0';
    }

    len = ush_strlen(line);

    while (len > 0ULL && ush_is_space(line[len - 1ULL]) != 0) {
        line[len - 1ULL] = '\0';
        len--;
    }
}

void ush_parse_line(const char *line, char *out_cmd, u64 cmd_size, char *out_arg, u64 arg_size) {
    u64 i = 0ULL;
    u64 cmd_pos = 0ULL;
    u64 arg_pos = 0ULL;

    if (line == (const char *)0 || out_cmd == (char *)0 || out_arg == (char *)0) {
        return;
    }

    out_cmd[0] = '\0';
    out_arg[0] = '\0';

    while (line[i] != '\0' && ush_is_space(line[i]) != 0) {
        i++;
    }

    while (line[i] != '\0' && ush_is_space(line[i]) == 0) {
        if (cmd_pos + 1ULL < cmd_size) {
            out_cmd[cmd_pos++] = line[i];
        }
        i++;
    }

    out_cmd[cmd_pos] = '\0';

    while (line[i] != '\0' && ush_is_space(line[i]) != 0) {
        i++;
    }

    while (line[i] != '\0') {
        if (arg_pos + 1ULL < arg_size) {
            out_arg[arg_pos++] = line[i];
        }
        i++;
    }

    out_arg[arg_pos] = '\0';
}

void ush_write(const char *text) {
    u64 len;

    if (text == (const char *)0) {
        return;
    }

    len = ush_strlen(text);

    if (len == 0ULL) {
        return;
    }

    (void)cleonos_sys_tty_write(text, len);
}

void ush_write_char(char ch) {
    (void)cleonos_sys_tty_write_char(ch);
}

void ush_writeln(const char *text) {
    ush_write(text);
    ush_write_char('\n');
}

void ush_prompt(const ush_state *sh) {
    if (sh == (const ush_state *)0) {
        ush_write("cleonos(user)> ");
        return;
    }

    ush_write("cleonos(user:");
    ush_write(sh->cwd);
    ush_write(")> ");
}

void ush_write_hex_u64(u64 value) {
    i64 nibble;

    ush_write("0X");

    for (nibble = 15; nibble >= 0; nibble--) {
        u64 current = (value >> (u64)(nibble * 4)) & 0x0FULL;
        char out = (current < 10ULL) ? (char)('0' + current) : (char)('A' + (current - 10ULL));
        ush_write_char(out);
    }
}

void ush_print_kv_hex(const char *label, u64 value) {
    ush_write(label);
    ush_write(": ");
    ush_write_hex_u64(value);
    ush_write_char('\n');
}