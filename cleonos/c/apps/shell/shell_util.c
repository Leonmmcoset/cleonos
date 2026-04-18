#include "shell_internal.h"

void ush_init_state(ush_state *sh) {
    if (sh == (ush_state *)0) {
        return;
    }

    (void)memset(sh, 0, sizeof(*sh));

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

static char *ush_out_capture_buffer = (char *)0;
static u64 ush_out_capture_capacity = 0ULL;
static u64 ush_out_capture_length = 0ULL;
static int ush_out_capture_active = 0;
static int ush_out_capture_mirror_tty = 1;
static int ush_out_capture_truncated = 0;
static u64 ush_out_fd = (u64)-1;
static int ush_out_fd_active = 0;
static int ush_out_fd_mirror_tty = 1;

static void ush_output_capture_append(const char *text, u64 len) {
    u64 writable;

    if (ush_out_capture_active == 0 || text == (const char *)0 || len == 0ULL) {
        return;
    }

    if (ush_out_capture_buffer == (char *)0 || ush_out_capture_capacity == 0ULL) {
        ush_out_capture_truncated = 1;
        return;
    }

    if (ush_out_capture_length + 1ULL >= ush_out_capture_capacity) {
        ush_out_capture_truncated = 1;
        return;
    }

    writable = (ush_out_capture_capacity - 1ULL) - ush_out_capture_length;

    if (len > writable) {
        len = writable;
        ush_out_capture_truncated = 1;
    }

    (void)memcpy(ush_out_capture_buffer + ush_out_capture_length, text, (size_t)len);

    ush_out_capture_length += len;
    ush_out_capture_buffer[ush_out_capture_length] = '\0';
}

void ush_output_capture_begin(char *buffer, u64 buffer_size, int mirror_to_tty) {
    ush_out_capture_buffer = buffer;
    ush_out_capture_capacity = buffer_size;
    ush_out_capture_length = 0ULL;
    ush_out_capture_active = 1;
    ush_out_capture_mirror_tty = (mirror_to_tty != 0) ? 1 : 0;
    ush_out_capture_truncated = 0;

    if (ush_out_capture_buffer != (char *)0 && ush_out_capture_capacity > 0ULL) {
        ush_out_capture_buffer[0] = '\0';
    }
}

u64 ush_output_capture_end(void) {
    u64 captured = ush_out_capture_length;

    if (ush_out_capture_buffer != (char *)0 && ush_out_capture_capacity > 0ULL) {
        if (ush_out_capture_length >= ush_out_capture_capacity) {
            ush_out_capture_length = ush_out_capture_capacity - 1ULL;
        }
        ush_out_capture_buffer[ush_out_capture_length] = '\0';
    }

    ush_out_capture_buffer = (char *)0;
    ush_out_capture_capacity = 0ULL;
    ush_out_capture_length = 0ULL;
    ush_out_capture_active = 0;
    ush_out_capture_mirror_tty = 1;

    return captured;
}

int ush_output_capture_truncated(void) {
    return ush_out_capture_truncated;
}

void ush_output_fd_begin(u64 fd, int mirror_to_tty) {
    ush_out_fd = fd;
    ush_out_fd_active = 1;
    ush_out_fd_mirror_tty = (mirror_to_tty != 0) ? 1 : 0;
}

void ush_output_fd_end(void) {
    ush_out_fd = (u64)-1;
    ush_out_fd_active = 0;
    ush_out_fd_mirror_tty = 1;
}

void ush_write(const char *text) {
    u64 len;
    int should_write_tty = 1;

    if (text == (const char *)0) {
        return;
    }

    len = ush_strlen(text);

    if (len == 0ULL) {
        return;
    }

    if (ush_out_capture_active != 0) {
        ush_output_capture_append(text, len);

        if (ush_out_capture_mirror_tty == 0) {
            should_write_tty = 0;
        }
    }

    if (ush_out_fd_active != 0 && ush_out_fd != (u64)-1) {
        (void)cleonos_sys_fd_write(ush_out_fd, text, len);

        if (ush_out_fd_mirror_tty == 0) {
            should_write_tty = 0;
        }
    }

    if (should_write_tty != 0) {
        (void)fputs(text, 1);
    }
}

void ush_write_char(char ch) {
    int should_write_tty = 1;

    if (ush_out_capture_active != 0) {
        ush_output_capture_append(&ch, 1ULL);

        if (ush_out_capture_mirror_tty == 0) {
            should_write_tty = 0;
        }
    }

    if (ush_out_fd_active != 0 && ush_out_fd != (u64)-1) {
        (void)cleonos_sys_fd_write(ush_out_fd, &ch, 1ULL);

        if (ush_out_fd_mirror_tty == 0) {
            should_write_tty = 0;
        }
    }

    if (should_write_tty != 0) {
        (void)fputc((int)(unsigned char)ch, 1);
    }
}

void ush_writeln(const char *text) {
    ush_write(text);
    ush_write_char('\n');
}

void ush_prompt(const ush_state *sh) {
    if (sh == (const ush_state *)0) {
        ush_write("\x1B[96mcleonos\x1B[0m(\x1B[92muser\x1B[0m)> ");
        return;
    }

    ush_write("\x1B[96mcleonos\x1B[0m(\x1B[92muser\x1B[0m:");
    ush_write("\x1B[93m");
    ush_write(sh->cwd);
    ush_write("\x1B[0m)> ");
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
