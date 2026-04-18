#include "cmd_runtime.h"

const char *ush_pipeline_stdin_text = (const char *)0;
u64 ush_pipeline_stdin_len = 0ULL;
static char ush_pipeline_stdin_buf[USH_COPY_MAX + 1U];

static int ush_cmd_runtime_has_prefix(const char *text, const char *prefix) {
    u64 i = 0ULL;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static int ush_cmd_runtime_stdin_pipe_enabled(char **envp) {
    u64 i = 0ULL;

    if (envp == (char **)0) {
        return 0;
    }

    while (envp[i] != (char *)0) {
        const char *entry = envp[i];

        if (ush_cmd_runtime_has_prefix(entry, "USH_STDIN_MODE=PIPE") != 0) {
            return 1;
        }

        i++;
    }

    return 0;
}

static void ush_cmd_runtime_capture_stdin_pipe(void) {
    u64 total = 0ULL;
    int truncated = 0;
    char drain[256];

    for (;;) {
        u64 got;

        if (total < (u64)USH_COPY_MAX) {
            got = cleonos_sys_fd_read(0ULL, ush_pipeline_stdin_buf + total, (u64)USH_COPY_MAX - total);
        } else {
            got = cleonos_sys_fd_read(0ULL, drain, (u64)sizeof(drain));
            truncated = 1;
        }

        if (got == (u64)-1 || got == 0ULL) {
            break;
        }

        if (total < (u64)USH_COPY_MAX) {
            total += got;
        }
    }

    ush_pipeline_stdin_buf[total] = '\0';
    ush_pipeline_stdin_text = ush_pipeline_stdin_buf;
    ush_pipeline_stdin_len = total;

    if (truncated != 0) {
        ush_writeln("[pipe] input truncated");
    }
}

void cleonos_cmd_runtime_pre_main(char **envp) {
    ush_pipeline_stdin_text = (const char *)0;
    ush_pipeline_stdin_len = 0ULL;
    ush_pipeline_stdin_buf[0] = '\0';

    if (ush_cmd_runtime_stdin_pipe_enabled(envp) != 0) {
        ush_cmd_runtime_capture_stdin_pipe();
    }
}

void ush_zero(void *ptr, u64 size) {
    if (ptr == (void *)0 || size == 0ULL) {
        return;
    }
    (void)memset(ptr, 0, (size_t)size);
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
    return (str == (const char *)0) ? 0ULL : (u64)strlen(str);
}

int ush_streq(const char *left, const char *right) {
    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    return (strcmp(left, right) == 0) ? 1 : 0;
}

int ush_is_space(char ch) {
    return (isspace((unsigned char)ch) != 0) ? 1 : 0;
}

int ush_is_printable(char ch) {
    return (isprint((unsigned char)ch) != 0) ? 1 : 0;
}

int ush_has_suffix(const char *name, const char *suffix) {
    size_t name_len;
    size_t suffix_len;

    if (name == (const char *)0 || suffix == (const char *)0) {
        return 0;
    }

    name_len = strlen(name);
    suffix_len = strlen(suffix);

    if (suffix_len > name_len) {
        return 0;
    }

    return (strncmp(name + (name_len - suffix_len), suffix, suffix_len) == 0) ? 1 : 0;
}

int ush_contains_char(const char *text, char needle) {
    if (text == (const char *)0) {
        return 0;
    }
    return (strchr(text, (int)needle) != (char *)0) ? 1 : 0;
}

int ush_parse_u64_dec(const char *text, u64 *out_value) {
    u64 value = 0ULL;
    u64 i = 0ULL;

    if (text == (const char *)0 || out_value == (u64 *)0 || text[0] == '\0') {
        return 0;
    }

    while (text[i] != '\0') {
        u64 digit;

        if (isdigit((unsigned char)text[i]) == 0) {
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
    if (dst == (char *)0 || src == (const char *)0 || dst_size == 0ULL) {
        return;
    }
    (void)strncpy(dst, src, (size_t)(dst_size - 1ULL));
    dst[dst_size - 1ULL] = '\0';
}

void ush_trim_line(char *line) {
    size_t start = 0U;
    size_t len;

    if (line == (char *)0) {
        return;
    }

    while (line[start] != '\0' && isspace((unsigned char)line[start]) != 0) {
        start++;
    }

    if (start > 0U) {
        size_t remain = strlen(line + start) + 1U;
        (void)memmove(line, line + start, remain);
    }

    len = strlen(line);

    while (len > 0U && isspace((unsigned char)line[len - 1U]) != 0) {
        line[len - 1U] = '\0';
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
    if (text == (const char *)0) {
        return;
    }
    (void)fputs(text, 1);
}

void ush_write_char(char ch) {
    (void)fputc((int)(unsigned char)ch, 1);
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
    if (strncmp(path, "/system", 7U) != 0) {
        return 0;
    }
    return (path[7] == '\0' || path[7] == '/') ? 1 : 0;
}

int ush_path_is_under_temp(const char *path) {
    if (path == (const char *)0) {
        return 0;
    }
    if (strncmp(path, "/temp", 5U) != 0) {
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

int ush_split_two_args(const char *arg, char *out_first, u64 out_first_size, char *out_second, u64 out_second_size) {
    u64 i = 0ULL;
    u64 p = 0ULL;

    if (arg == (const char *)0 || out_first == (char *)0 || out_first_size == 0ULL || out_second == (char *)0 ||
        out_second_size == 0ULL) {
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
