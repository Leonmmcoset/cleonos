#include <clks/log.h>
#include <clks/serial.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_LOG_LINE_MAX 256
#define CLKS_LOG_JOURNAL_CAP 256

#define CLKS_LOG_ANSI_RESET "\x1B[0m"

static char clks_log_journal[CLKS_LOG_JOURNAL_CAP][CLKS_LOG_LINE_MAX];
static u32 clks_log_journal_head = 0U;
static u32 clks_log_journal_count_live = 0U;

static const char *clks_log_level_name(enum clks_log_level level) {
    switch (level) {
    case CLKS_LOG_DEBUG:
        return "DEBUG";
    case CLKS_LOG_INFO:
        return "INFO";
    case CLKS_LOG_WARN:
        return "WARN";
    case CLKS_LOG_ERROR:
        return "ERROR";
    default:
        return "UNK";
    }
}

static void clks_log_append_char(char *buffer, usize *cursor, char ch) {
    if (*cursor >= (CLKS_LOG_LINE_MAX - 1)) {
        return;
    }

    buffer[*cursor] = ch;
    (*cursor)++;
}

static void clks_log_append_text(char *buffer, usize *cursor, const char *text) {
    usize i = 0;

    if (text == CLKS_NULL) {
        return;
    }

    while (text[i] != '\0') {
        clks_log_append_char(buffer, cursor, text[i]);
        i++;
    }
}

static void clks_log_append_hex_u64(char *buffer, usize *cursor, u64 value) {
    int nibble;

    clks_log_append_text(buffer, cursor, "0X");

    for (nibble = 15; nibble >= 0; nibble--) {
        u8 current = (u8)((value >> (nibble * 4)) & 0x0FULL);
        char out = (current < 10) ? (char)('0' + current) : (char)('A' + (current - 10));
        clks_log_append_char(buffer, cursor, out);
    }
}

static void clks_log_journal_copy_line(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || src == CLKS_NULL || dst_size == 0U) {
        return;
    }

    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void clks_log_journal_push(const char *line) {
    if (line == CLKS_NULL) {
        return;
    }

    clks_log_journal_copy_line(clks_log_journal[clks_log_journal_head], CLKS_LOG_LINE_MAX, line);
    clks_log_journal_head = (clks_log_journal_head + 1U) % CLKS_LOG_JOURNAL_CAP;

    if (clks_log_journal_count_live < CLKS_LOG_JOURNAL_CAP) {
        clks_log_journal_count_live++;
    }
}

static const char *clks_log_level_ansi(enum clks_log_level level) {
    switch (level) {
    case CLKS_LOG_DEBUG:
        return "\x1B[38;5;110m";
    case CLKS_LOG_INFO:
        return "\x1B[38;5;120m";
    case CLKS_LOG_WARN:
        return "\x1B[1;38;5;220m";
    case CLKS_LOG_ERROR:
        return "\x1B[1;38;5;203m";
    default:
        return "\x1B[38;5;250m";
    }
}

static const char *clks_log_tag_ansi(const char *tag) {
    static const char *palette[] = {
        "\x1B[38;5;81m", "\x1B[38;5;117m", "\x1B[38;5;159m", "\x1B[38;5;45m",
        "\x1B[38;5;75m", "\x1B[38;5;141m", "\x1B[38;5;214m", "\x1B[38;5;168m",
    };
    u32 hash = 5381U;
    usize i = 0U;
    usize palette_count = sizeof(palette) / sizeof(palette[0]);

    if (tag == CLKS_NULL || tag[0] == '\0') {
        return palette[0];
    }

    while (tag[i] != '\0') {
        hash = ((hash << 5U) + hash) ^ (u32)(u8)tag[i];
        i++;
    }

    return palette[hash % (u32)palette_count];
}

static void clks_log_emit_tty_colored(enum clks_log_level level, const char *tag, const char *message) {
    const char *safe_tag = (tag == CLKS_NULL) ? "LOG" : tag;
    const char *safe_message = (message == CLKS_NULL) ? "" : message;

    clks_tty_write(clks_log_level_ansi(level));
    clks_tty_write("[");
    clks_tty_write(clks_log_level_name(level));
    clks_tty_write("]");

    clks_tty_write(clks_log_tag_ansi(safe_tag));
    clks_tty_write("[");
    clks_tty_write(safe_tag);
    clks_tty_write("]");

    clks_tty_write(CLKS_LOG_ANSI_RESET);
    clks_tty_write(" ");
    clks_tty_write(safe_message);
    clks_tty_write("\n");
}

static void clks_log_build_line(enum clks_log_level level, const char *tag, const char *message, char *line) {
    const char *safe_tag = (tag == CLKS_NULL) ? "LOG" : tag;
    const char *safe_message = (message == CLKS_NULL) ? "" : message;
    usize cursor = 0U;

    if (line == CLKS_NULL) {
        return;
    }

    clks_log_append_char(line, &cursor, '[');
    clks_log_append_text(line, &cursor, clks_log_level_name(level));
    clks_log_append_char(line, &cursor, ']');
    clks_log_append_char(line, &cursor, '[');
    clks_log_append_text(line, &cursor, safe_tag);
    clks_log_append_char(line, &cursor, ']');
    clks_log_append_char(line, &cursor, ' ');
    clks_log_append_text(line, &cursor, safe_message);
    line[cursor] = '\0';
}

static void clks_log_emit_line(enum clks_log_level level, const char *tag, const char *message, const char *line) {
    if (line == CLKS_NULL) {
        return;
    }

    clks_log_journal_push(line);

    clks_serial_write(line);
    clks_serial_write("\n");

    clks_log_emit_tty_colored(level, tag, message);
}

void clks_log(enum clks_log_level level, const char *tag, const char *message) {
    char line[CLKS_LOG_LINE_MAX];

    clks_log_build_line(level, tag, message, line);
    clks_log_emit_line(level, tag, message, line);
}

void clks_log_hex(enum clks_log_level level, const char *tag, const char *label, u64 value) {
    char message[CLKS_LOG_LINE_MAX];
    char line[CLKS_LOG_LINE_MAX];
    usize cursor = 0U;

    clks_log_append_text(message, &cursor, (label == CLKS_NULL) ? "VALUE" : label);
    clks_log_append_char(message, &cursor, ':');
    clks_log_append_char(message, &cursor, ' ');
    clks_log_append_hex_u64(message, &cursor, value);
    message[cursor] = '\0';

    clks_log_build_line(level, tag, message, line);
    clks_log_emit_line(level, tag, message, line);
}

u64 clks_log_journal_count(void) {
    return (u64)clks_log_journal_count_live;
}

clks_bool clks_log_journal_read(u64 index_from_oldest, char *out_line, usize out_line_size) {
    u32 oldest;
    u32 slot;

    if (out_line == CLKS_NULL || out_line_size == 0U) {
        return CLKS_FALSE;
    }

    out_line[0] = '\0';

    if (index_from_oldest >= (u64)clks_log_journal_count_live) {
        return CLKS_FALSE;
    }

    oldest = (clks_log_journal_head + CLKS_LOG_JOURNAL_CAP - clks_log_journal_count_live) % CLKS_LOG_JOURNAL_CAP;
    slot = (oldest + (u32)index_from_oldest) % CLKS_LOG_JOURNAL_CAP;

    clks_log_journal_copy_line(out_line, out_line_size, clks_log_journal[slot]);
    return CLKS_TRUE;
}
