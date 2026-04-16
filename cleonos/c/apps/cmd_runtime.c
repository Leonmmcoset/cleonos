#include "cmd_runtime.h"

const char *ush_pipeline_stdin_text = (const char *)0;
u64 ush_pipeline_stdin_len = 0ULL;

void ush_zero(void *ptr, u64 size) {
    u64 i;
    char *bytes = (char *)ptr;

    if (bytes == (char *)0) {
        return;
    }

    for (i = 0ULL; i < size; i++) {
        bytes[i] = 0;
    }
}

void ush_init_state(ush_state *sh) {
    if (sh == (ush_state *)0) {
        return;
    }

    ush_zero(sh, (u64)sizeof(*sh));
    ush_copy(sh->cwd, (u64)sizeof(sh->cwd), "/");
    sh->history_nav = -1;
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

static int ush_path_push_component(char *path, u64 path_size, u64 *io_len, const char *component, u64 comp_len) {
    u64 i;

    if (path == (char *)0 || io_len == (u64 *)0 || component == (const char *)0 || comp_len == 0ULL) {
        return 0;
    }

    if (*io_len == 1ULL) {
        if (*io_len + comp_len >= path_size) {
            return 0;
        }

        for (i = 0ULL; i < comp_len; i++) {
            path[1ULL + i] = component[i];
        }

        *io_len = 1ULL + comp_len;
        path[*io_len] = '\0';
        return 1;
    }

    if (*io_len + 1ULL + comp_len >= path_size) {
        return 0;
    }

    path[*io_len] = '/';
    for (i = 0ULL; i < comp_len; i++) {
        path[*io_len + 1ULL + i] = component[i];
    }

    *io_len += (1ULL + comp_len);
    path[*io_len] = '\0';
    return 1;
}

static void ush_path_pop_component(char *path, u64 *io_len) {
    if (path == (char *)0 || io_len == (u64 *)0) {
        return;
    }

    if (*io_len <= 1ULL) {
        path[0] = '/';
        path[1] = '\0';
        *io_len = 1ULL;
        return;
    }

    while (*io_len > 1ULL && path[*io_len - 1ULL] != '/') {
        (*io_len)--;
    }

    if (*io_len > 1ULL) {
        (*io_len)--;
    }

    path[*io_len] = '\0';
}

static int ush_path_parse_into(const char *src, char *out_path, u64 out_size, u64 *io_len) {
    u64 i = 0ULL;

    if (src == (const char *)0 || out_path == (char *)0 || io_len == (u64 *)0) {
        return 0;
    }

    if (src[0] == '/') {
        i = 1ULL;
    }

    while (src[i] != '\0') {
        u64 start;
        u64 len;

        while (src[i] == '/') {
            i++;
        }

        if (src[i] == '\0') {
            break;
        }

        start = i;

        while (src[i] != '\0' && src[i] != '/') {
            i++;
        }

        len = i - start;

        if (len == 1ULL && src[start] == '.') {
            continue;
        }

        if (len == 2ULL && src[start] == '.' && src[start + 1ULL] == '.') {
            ush_path_pop_component(out_path, io_len);
            continue;
        }

        if (ush_path_push_component(out_path, out_size, io_len, src + start, len) == 0) {
            return 0;
        }
    }

    return 1;
}

int ush_resolve_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size) {
    u64 len = 1ULL;

    if (sh == (const ush_state *)0 || out_path == (char *)0 || out_size < 2ULL) {
        return 0;
    }

    out_path[0] = '/';
    out_path[1] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return ush_path_parse_into(sh->cwd, out_path, out_size, &len);
    }

    if (arg[0] != '/') {
        if (ush_path_parse_into(sh->cwd, out_path, out_size, &len) == 0) {
            return 0;
        }
    }

    return ush_path_parse_into(arg, out_path, out_size, &len);
}

int ush_resolve_exec_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size) {
    u64 i;
    u64 cursor = 0ULL;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (arg[0] == '\0') {
        return 0;
    }

    out_path[0] = '\0';

    if (arg[0] == '/') {
        ush_copy(out_path, out_size, arg);
    } else if (ush_contains_char(arg, '/') != 0) {
        if (ush_resolve_path(sh, arg, out_path, out_size) == 0) {
            return 0;
        }
    } else {
        static const char prefix[] = "/shell/";
        u64 prefix_len = (u64)(sizeof(prefix) - 1U);

        if (prefix_len + 1ULL >= out_size) {
            return 0;
        }

        for (i = 0ULL; i < prefix_len; i++) {
            out_path[cursor++] = prefix[i];
        }

        for (i = 0ULL; arg[i] != '\0'; i++) {
            if (cursor + 1ULL >= out_size) {
                return 0;
            }
            out_path[cursor++] = arg[i];
        }

        out_path[cursor] = '\0';
    }

    if (ush_has_suffix(out_path, ".elf") == 0) {
        static const char suffix[] = ".elf";

        cursor = ush_strlen(out_path);

        for (i = 0ULL; suffix[i] != '\0'; i++) {
            if (cursor + 1ULL >= out_size) {
                return 0;
            }
            out_path[cursor++] = suffix[i];
        }

        out_path[cursor] = '\0';
    }

    return 1;
}

int ush_path_is_under_system(const char *path) {
    if (path == (const char *)0) {
        return 0;
    }

    if (path[0] != '/' || path[1] != 's' || path[2] != 'y' || path[3] != 's' || path[4] != 't' || path[5] != 'e' || path[6] != 'm') {
        return 0;
    }

    return (path[7] == '\0' || path[7] == '/') ? 1 : 0;
}

int ush_path_is_under_temp(const char *path) {
    if (path == (const char *)0) {
        return 0;
    }

    if (path[0] != '/' || path[1] != 't' || path[2] != 'e' || path[3] != 'm' || path[4] != 'p') {
        return 0;
    }

    return (path[5] == '\0' || path[5] == '/') ? 1 : 0;
}

int ush_split_first_and_rest(const char *arg, char *out_first, u64 out_first_size, const char **out_rest) {
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

int ush_split_two_args(const char *arg,
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

int ush_command_ctx_read(ush_cmd_ctx *out_ctx) {
    u64 got;

    if (out_ctx == (ush_cmd_ctx *)0) {
        return 0;
    }

    ush_zero(out_ctx, (u64)sizeof(*out_ctx));
    got = cleonos_sys_fs_read(USH_CMD_CTX_PATH, (char *)out_ctx, (u64)sizeof(*out_ctx));
    return (got == (u64)sizeof(*out_ctx)) ? 1 : 0;
}

int ush_command_ret_write(const ush_cmd_ret *ret) {
    if (ret == (const ush_cmd_ret *)0) {
        return 0;
    }

    return (cleonos_sys_fs_write(USH_CMD_RET_PATH, (const char *)ret, (u64)sizeof(*ret)) != 0ULL) ? 1 : 0;
}
