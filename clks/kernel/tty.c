#include <clks/exec.h>
#include <clks/framebuffer.h>
#include <clks/string.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_TTY_COUNT 4
#define CLKS_TTY_MAX_ROWS 128
#define CLKS_TTY_MAX_COLS 256

#define CLKS_TTY_FG 0x00E6E6E6U
#define CLKS_TTY_BG 0x00101010U
#define CLKS_TTY_CURSOR_BLINK_INTERVAL_TICKS 5ULL
#define CLKS_TTY_BLINK_TICK_UNSET 0xFFFFFFFFFFFFFFFFULL
#define CLKS_TTY_DESKTOP_INDEX 1U
#define CLKS_TTY_ANSI_MAX_LEN 95U
#define CLKS_TTY_SCROLLBACK_LINES 256U
#define CLKS_TTY_STYLE_NONE 0U
#define CLKS_TTY_STYLE_BOLD ((u8)CLKS_FB_STYLE_BOLD)
#define CLKS_TTY_STYLE_UNDERLINE ((u8)CLKS_FB_STYLE_UNDERLINE)
#define CLKS_TTY_STATUS_BG 0x00202020U
#define CLKS_TTY_STATUS_FG 0x00E6E6E6U
#define CLKS_TTY_STATUS_STYLE CLKS_TTY_STYLE_BOLD

typedef struct clks_tty_ansi_state {
    clks_bool in_escape;
    clks_bool saw_csi;
    clks_bool bold;
    clks_bool underline;
    clks_bool inverse;
    u32 saved_row;
    u32 saved_col;
    u32 len;
    char params[CLKS_TTY_ANSI_MAX_LEN + 1U];
} clks_tty_ansi_state;

static char clks_tty_cells[CLKS_TTY_COUNT][CLKS_TTY_MAX_ROWS][CLKS_TTY_MAX_COLS];
static u32 clks_tty_cell_fg[CLKS_TTY_COUNT][CLKS_TTY_MAX_ROWS][CLKS_TTY_MAX_COLS];
static u32 clks_tty_cell_bg[CLKS_TTY_COUNT][CLKS_TTY_MAX_ROWS][CLKS_TTY_MAX_COLS];
static u8 clks_tty_cell_style[CLKS_TTY_COUNT][CLKS_TTY_MAX_ROWS][CLKS_TTY_MAX_COLS];
static u32 clks_tty_cursor_row[CLKS_TTY_COUNT];
static u32 clks_tty_cursor_col[CLKS_TTY_COUNT];
static u32 clks_tty_current_fg[CLKS_TTY_COUNT];
static u32 clks_tty_current_bg[CLKS_TTY_COUNT];
static clks_tty_ansi_state clks_tty_ansi[CLKS_TTY_COUNT];
static char clks_tty_scrollback_cells[CLKS_TTY_COUNT][CLKS_TTY_SCROLLBACK_LINES][CLKS_TTY_MAX_COLS];
static u32 clks_tty_scrollback_fg[CLKS_TTY_COUNT][CLKS_TTY_SCROLLBACK_LINES][CLKS_TTY_MAX_COLS];
static u32 clks_tty_scrollback_bg[CLKS_TTY_COUNT][CLKS_TTY_SCROLLBACK_LINES][CLKS_TTY_MAX_COLS];
static u8 clks_tty_scrollback_style[CLKS_TTY_COUNT][CLKS_TTY_SCROLLBACK_LINES][CLKS_TTY_MAX_COLS];
static u32 clks_tty_scrollback_head[CLKS_TTY_COUNT];
static u32 clks_tty_scrollback_count[CLKS_TTY_COUNT];
static u32 clks_tty_scrollback_offset[CLKS_TTY_COUNT];

static u32 clks_tty_rows = 0;
static u32 clks_tty_cols = 0;
static u32 clks_tty_active_index = 0;
static u32 clks_tty_cell_width = 8U;
static u32 clks_tty_cell_height = 8U;
static clks_bool clks_tty_is_ready = CLKS_FALSE;
static clks_bool clks_tty_cursor_visible = CLKS_FALSE;
static clks_bool clks_tty_blink_enabled = CLKS_TRUE;
static u64 clks_tty_blink_last_tick = CLKS_TTY_BLINK_TICK_UNSET;

static u32 clks_tty_ansi_palette(u32 index) {
    static const u32 palette[16] = {
        0x00000000U, 0x00CD3131U, 0x000DBC79U, 0x00E5E510U,
        0x002472C8U, 0x00BC3FBCU, 0x0011A8CDU, 0x00E5E5E5U,
        0x00666666U, 0x00F14C4CU, 0x0023D18BU, 0x00F5F543U,
        0x003B8EEAU, 0x00D670D6U, 0x0029B8DBU, 0x00FFFFFFU
    };

    if (index < 16U) {
        return palette[index];
    }

    return CLKS_TTY_FG;
}

static void clks_tty_reset_blink_timer(void) {
    clks_tty_blink_last_tick = CLKS_TTY_BLINK_TICK_UNSET;
}

static void clks_tty_draw_cell_with_colors(u32 row, u32 col, char ch, u32 fg, u32 bg, u8 style) {
    clks_fb_draw_char_styled(
        col * clks_tty_cell_width,
        row * clks_tty_cell_height,
        ch,
        fg,
        bg,
        (u32)style
    );
}

static void clks_tty_draw_cell(u32 tty_index, u32 row, u32 col) {
    clks_tty_draw_cell_with_colors(
        row,
        col,
        clks_tty_cells[tty_index][row][col],
        clks_tty_cell_fg[tty_index][row][col],
        clks_tty_cell_bg[tty_index][row][col],
        clks_tty_cell_style[tty_index][row][col]
    );
}

static u32 clks_tty_scrollback_logical_to_physical(u32 tty_index, u32 logical_index) {
    u32 count = clks_tty_scrollback_count[tty_index];

    if (count < CLKS_TTY_SCROLLBACK_LINES) {
        return logical_index;
    }

    return (clks_tty_scrollback_head[tty_index] + logical_index) % CLKS_TTY_SCROLLBACK_LINES;
}

static u32 clks_tty_scrollback_max_offset(u32 tty_index) {
    return clks_tty_scrollback_count[tty_index];
}

static u32 clks_tty_scrollback_clamped_offset(u32 tty_index) {
    u32 max_offset = clks_tty_scrollback_max_offset(tty_index);

    if (clks_tty_scrollback_offset[tty_index] > max_offset) {
        clks_tty_scrollback_offset[tty_index] = max_offset;
    }

    return clks_tty_scrollback_offset[tty_index];
}

static void clks_tty_scrollback_push_row(u32 tty_index, u32 row) {
    u32 slot = clks_tty_scrollback_head[tty_index];

    clks_memcpy(clks_tty_scrollback_cells[tty_index][slot], clks_tty_cells[tty_index][row], clks_tty_cols);
    clks_memcpy(
        clks_tty_scrollback_fg[tty_index][slot],
        clks_tty_cell_fg[tty_index][row],
        (usize)clks_tty_cols * sizeof(u32)
    );
    clks_memcpy(
        clks_tty_scrollback_bg[tty_index][slot],
        clks_tty_cell_bg[tty_index][row],
        (usize)clks_tty_cols * sizeof(u32)
    );
    clks_memcpy(
        clks_tty_scrollback_style[tty_index][slot],
        clks_tty_cell_style[tty_index][row],
        clks_tty_cols
    );

    clks_tty_scrollback_head[tty_index] = (slot + 1U) % CLKS_TTY_SCROLLBACK_LINES;

    if (clks_tty_scrollback_count[tty_index] < CLKS_TTY_SCROLLBACK_LINES) {
        clks_tty_scrollback_count[tty_index]++;
    }
}

static clks_bool clks_tty_scrollback_is_active(u32 tty_index) {
    return (clks_tty_scrollback_offset[tty_index] > 0U) ? CLKS_TRUE : CLKS_FALSE;
}

static void clks_tty_scrollback_follow_tail(u32 tty_index) {
    clks_tty_scrollback_offset[tty_index] = 0U;
}

static u32 clks_tty_content_rows(void) {
    if (clks_tty_rows > 1U) {
        return clks_tty_rows - 1U;
    }

    return clks_tty_rows;
}

static clks_bool clks_tty_status_bar_enabled(void) {
    return (clks_tty_rows > 1U) ? CLKS_TRUE : CLKS_FALSE;
}

static void clks_tty_status_append_char(char *line, u32 line_size, u32 *cursor, char ch) {
    if (line == CLKS_NULL || cursor == CLKS_NULL || *cursor >= line_size) {
        return;
    }

    line[*cursor] = ch;
    (*cursor)++;
}

static void clks_tty_status_append_text(char *line, u32 line_size, u32 *cursor, const char *text) {
    u32 i = 0U;

    if (text == CLKS_NULL) {
        return;
    }

    while (text[i] != '\0') {
        clks_tty_status_append_char(line, line_size, cursor, text[i]);
        i++;
    }
}

static void clks_tty_status_append_u32_dec(char *line, u32 line_size, u32 *cursor, u32 value) {
    char tmp[10];
    u32 len = 0U;

    if (value == 0U) {
        clks_tty_status_append_char(line, line_size, cursor, '0');
        return;
    }

    while (value > 0U && len < (u32)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (len > 0U) {
        len--;
        clks_tty_status_append_char(line, line_size, cursor, tmp[len]);
    }
}

static void clks_tty_draw_status_bar(void) {
    u32 tty_index;
    u32 status_row;
    u32 content_rows;
    u32 cursor = 0U;
    u32 col;
    u32 shown_row;
    u32 shown_col;
    u32 scroll_offset;
    const char *run_mode = "KERNEL";
    const char *input_mode;
    char line[CLKS_TTY_MAX_COLS + 1U];

    if (clks_tty_is_ready == CLKS_FALSE || clks_tty_status_bar_enabled() == CLKS_FALSE) {
        return;
    }

    if (clks_tty_active_index == CLKS_TTY_DESKTOP_INDEX) {
        return;
    }

    tty_index = clks_tty_active_index;
    content_rows = clks_tty_content_rows();
    status_row = clks_tty_rows - 1U;

    for (col = 0U; col < clks_tty_cols && col < CLKS_TTY_MAX_COLS; col++) {
        line[col] = ' ';
    }

    if (clks_tty_cols < CLKS_TTY_MAX_COLS) {
        line[clks_tty_cols] = '\0';
    } else {
        line[CLKS_TTY_MAX_COLS] = '\0';
    }

    shown_row = clks_tty_cursor_row[tty_index] + 1U;
    shown_col = clks_tty_cursor_col[tty_index] + 1U;
    scroll_offset = clks_tty_scrollback_clamped_offset(tty_index);
    input_mode = (scroll_offset > 0U) ? "SCROLL" : "INPUT";

    if (clks_exec_is_running() == CLKS_TRUE && clks_exec_current_tty() == tty_index) {
        run_mode = (clks_exec_current_path_is_user() == CLKS_TRUE) ? "USER" : "KAPP";
    }

    clks_tty_status_append_text(line, clks_tty_cols, &cursor, "TTY");
    clks_tty_status_append_u32_dec(line, clks_tty_cols, &cursor, tty_index);
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, " ");
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, "ROW:");
    if (shown_row > content_rows) {
        shown_row = content_rows;
    }
    clks_tty_status_append_u32_dec(line, clks_tty_cols, &cursor, shown_row);
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, " COL:");
    clks_tty_status_append_u32_dec(line, clks_tty_cols, &cursor, shown_col);
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, " SB:");
    clks_tty_status_append_u32_dec(line, clks_tty_cols, &cursor, scroll_offset);
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, " IN:");
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, input_mode);
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, " MODE:");
    clks_tty_status_append_text(line, clks_tty_cols, &cursor, run_mode);

    for (col = 0U; col < clks_tty_cols; col++) {
        char ch = (col < CLKS_TTY_MAX_COLS) ? line[col] : ' ';

        clks_tty_draw_cell_with_colors(
            status_row,
            col,
            ch,
            CLKS_TTY_STATUS_FG,
            CLKS_TTY_STATUS_BG,
            CLKS_TTY_STATUS_STYLE
        );
    }
}

static void clks_tty_reset_color_state(u32 tty_index) {
    clks_tty_current_fg[tty_index] = CLKS_TTY_FG;
    clks_tty_current_bg[tty_index] = CLKS_TTY_BG;
    clks_tty_ansi[tty_index].bold = CLKS_FALSE;
    clks_tty_ansi[tty_index].underline = CLKS_FALSE;
    clks_tty_ansi[tty_index].inverse = CLKS_FALSE;
}

static void clks_tty_reset_ansi_state(u32 tty_index) {
    clks_tty_ansi[tty_index].in_escape = CLKS_FALSE;
    clks_tty_ansi[tty_index].saw_csi = CLKS_FALSE;
    clks_tty_ansi[tty_index].len = 0U;
    clks_tty_ansi[tty_index].params[0] = '\0';
}

static void clks_tty_fill_row(u32 tty_index, u32 row, char ch) {
    u32 col;

    for (col = 0; col < clks_tty_cols; col++) {
        clks_tty_cells[tty_index][row][col] = ch;
        clks_tty_cell_fg[tty_index][row][col] = CLKS_TTY_FG;
        clks_tty_cell_bg[tty_index][row][col] = CLKS_TTY_BG;
        clks_tty_cell_style[tty_index][row][col] = CLKS_TTY_STYLE_NONE;
    }
}

static void clks_tty_clear_tty(u32 tty_index) {
    u32 row;

    for (row = 0; row < clks_tty_content_rows(); row++) {
        clks_tty_fill_row(tty_index, row, ' ');
    }

    clks_tty_cursor_row[tty_index] = 0U;
    clks_tty_cursor_col[tty_index] = 0U;
    clks_tty_scrollback_head[tty_index] = 0U;
    clks_tty_scrollback_count[tty_index] = 0U;
    clks_tty_scrollback_offset[tty_index] = 0U;
}

static void clks_tty_hide_cursor(void) {
    u32 row;
    u32 col;

    if (clks_tty_is_ready == CLKS_FALSE || clks_tty_cursor_visible == CLKS_FALSE) {
        return;
    }

    if (clks_tty_scrollback_is_active(clks_tty_active_index) == CLKS_TRUE) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    row = clks_tty_cursor_row[clks_tty_active_index];
    col = clks_tty_cursor_col[clks_tty_active_index];

    if (row < clks_tty_content_rows() && col < clks_tty_cols) {
        clks_tty_draw_cell(clks_tty_active_index, row, col);
    }

    clks_tty_cursor_visible = CLKS_FALSE;
}

static void clks_tty_draw_cursor(void) {
    u32 row;
    u32 col;
    u32 fg;
    u32 bg;
    u8 style;
    char ch;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    if (clks_tty_active_index == CLKS_TTY_DESKTOP_INDEX) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    if (clks_tty_scrollback_is_active(clks_tty_active_index) == CLKS_TRUE) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    row = clks_tty_cursor_row[clks_tty_active_index];
    col = clks_tty_cursor_col[clks_tty_active_index];

    if (row >= clks_tty_content_rows() || col >= clks_tty_cols) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    ch = clks_tty_cells[clks_tty_active_index][row][col];
    fg = clks_tty_cell_fg[clks_tty_active_index][row][col];
    bg = clks_tty_cell_bg[clks_tty_active_index][row][col];
    style = clks_tty_cell_style[clks_tty_active_index][row][col];

    clks_tty_draw_cell_with_colors(row, col, ch, bg, fg, style);
    clks_tty_cursor_visible = CLKS_TRUE;
}

static void clks_tty_redraw_active(void) {
    u32 row;
    u32 col;
    u32 tty_index = clks_tty_active_index;
    u32 scroll_count = clks_tty_scrollback_count[tty_index];
    u32 scroll_offset = clks_tty_scrollback_clamped_offset(tty_index);
    u32 start_doc = (scroll_count >= scroll_offset) ? (scroll_count - scroll_offset) : 0U;

    clks_fb_clear(CLKS_TTY_BG);
    clks_tty_cursor_visible = CLKS_FALSE;

    for (row = 0; row < clks_tty_content_rows(); row++) {
        u32 doc_index = start_doc + row;

        if (doc_index < scroll_count) {
            u32 phys = clks_tty_scrollback_logical_to_physical(tty_index, doc_index);

            for (col = 0; col < clks_tty_cols; col++) {
                clks_tty_draw_cell_with_colors(
                    row,
                    col,
                    clks_tty_scrollback_cells[tty_index][phys][col],
                    clks_tty_scrollback_fg[tty_index][phys][col],
                    clks_tty_scrollback_bg[tty_index][phys][col],
                    clks_tty_scrollback_style[tty_index][phys][col]
                );
            }

            continue;
        }

        {
            u32 src_row = doc_index - scroll_count;

            if (src_row >= clks_tty_content_rows()) {
                continue;
            }

            for (col = 0; col < clks_tty_cols; col++) {
                clks_tty_draw_cell_with_colors(
                    row,
                    col,
                    clks_tty_cells[tty_index][src_row][col],
                    clks_tty_cell_fg[tty_index][src_row][col],
                    clks_tty_cell_bg[tty_index][src_row][col],
                    clks_tty_cell_style[tty_index][src_row][col]
                );
            }
        }
    }

    if (scroll_offset == 0U) {
        clks_tty_draw_cursor();
    }

    clks_tty_draw_status_bar();
}
static void clks_tty_scroll_up(u32 tty_index) {
    u32 row;

    clks_tty_scrollback_push_row(tty_index, 0U);

    for (row = 1; row < clks_tty_content_rows(); row++) {
        clks_memcpy(clks_tty_cells[tty_index][row - 1U], clks_tty_cells[tty_index][row], clks_tty_cols);
        clks_memcpy(
            clks_tty_cell_fg[tty_index][row - 1U],
            clks_tty_cell_fg[tty_index][row],
            (usize)clks_tty_cols * sizeof(u32)
        );
        clks_memcpy(
            clks_tty_cell_bg[tty_index][row - 1U],
            clks_tty_cell_bg[tty_index][row],
            (usize)clks_tty_cols * sizeof(u32)
        );
        clks_memcpy(
            clks_tty_cell_style[tty_index][row - 1U],
            clks_tty_cell_style[tty_index][row],
            clks_tty_cols
        );
    }

    clks_tty_fill_row(tty_index, clks_tty_content_rows() - 1U, ' ');

    if (tty_index == clks_tty_active_index) {
        if (clks_tty_scrollback_is_active(tty_index) == CLKS_TRUE) {
            clks_tty_redraw_active();
        } else {
            u32 col;

            clks_fb_scroll_up(clks_tty_cell_height, CLKS_TTY_BG);

            for (col = 0U; col < clks_tty_cols; col++) {
                clks_tty_draw_cell(tty_index, clks_tty_content_rows() - 1U, col);
            }
        }
    }
}
static void clks_tty_put_visible(u32 tty_index, u32 row, u32 col, char ch) {
    u32 fg = clks_tty_current_fg[tty_index];
    u32 bg = clks_tty_current_bg[tty_index];
    u8 style = CLKS_TTY_STYLE_NONE;

    if (clks_tty_ansi[tty_index].inverse == CLKS_TRUE) {
        u32 swap = fg;
        fg = bg;
        bg = swap;
    }

    if (clks_tty_ansi[tty_index].bold == CLKS_TRUE) {
        style = (u8)(style | CLKS_TTY_STYLE_BOLD);
    }

    if (clks_tty_ansi[tty_index].underline == CLKS_TRUE) {
        style = (u8)(style | CLKS_TTY_STYLE_UNDERLINE);
    }

    clks_tty_cells[tty_index][row][col] = ch;
    clks_tty_cell_fg[tty_index][row][col] = fg;
    clks_tty_cell_bg[tty_index][row][col] = bg;
    clks_tty_cell_style[tty_index][row][col] = style;

    if (tty_index == clks_tty_active_index) {
        clks_tty_draw_cell(tty_index, row, col);
    }
}

static void clks_tty_put_char_raw(u32 tty_index, char ch) {
    u32 row = clks_tty_cursor_row[tty_index];
    u32 col = clks_tty_cursor_col[tty_index];

    if (ch == '\r') {
        clks_tty_cursor_col[tty_index] = 0U;
        return;
    }

    if (ch == '\n') {
        clks_tty_cursor_col[tty_index] = 0U;
        clks_tty_cursor_row[tty_index]++;

        if (clks_tty_cursor_row[tty_index] >= clks_tty_content_rows()) {
            clks_tty_scroll_up(tty_index);
            clks_tty_cursor_row[tty_index] = clks_tty_content_rows() - 1U;
        }

        return;
    }

    if (ch == '\b') {
        if (col == 0U && row == 0U) {
            return;
        }

        if (col == 0U) {
            row--;
            col = clks_tty_cols - 1U;
        } else {
            col--;
        }

        clks_tty_put_visible(tty_index, row, col, ' ');
        clks_tty_cursor_row[tty_index] = row;
        clks_tty_cursor_col[tty_index] = col;
        return;
    }

    if (ch == '\t') {
        clks_tty_put_char_raw(tty_index, ' ');
        clks_tty_put_char_raw(tty_index, ' ');
        clks_tty_put_char_raw(tty_index, ' ');
        clks_tty_put_char_raw(tty_index, ' ');
        return;
    }

    clks_tty_put_visible(tty_index, row, col, ch);
    clks_tty_cursor_col[tty_index]++;

    if (clks_tty_cursor_col[tty_index] >= clks_tty_cols) {
        clks_tty_cursor_col[tty_index] = 0U;
        clks_tty_cursor_row[tty_index]++;

        if (clks_tty_cursor_row[tty_index] >= clks_tty_content_rows()) {
            clks_tty_scroll_up(tty_index);
            clks_tty_cursor_row[tty_index] = clks_tty_content_rows() - 1U;
        }
    }
}

static u32 clks_tty_ansi_clamp_255(u32 value) {
    return (value > 255U) ? 255U : value;
}

static u32 clks_tty_ansi_color_from_256(u32 index) {
    if (index < 16U) {
        return clks_tty_ansi_palette(index);
    }

    if (index >= 16U && index <= 231U) {
        static const u32 steps[6] = {0U, 95U, 135U, 175U, 215U, 255U};
        u32 n = index - 16U;
        u32 r = n / 36U;
        u32 g = (n / 6U) % 6U;
        u32 b = n % 6U;

        return (steps[r] << 16U) | (steps[g] << 8U) | steps[b];
    }

    if (index >= 232U && index <= 255U) {
        u32 gray = 8U + ((index - 232U) * 10U);
        return (gray << 16U) | (gray << 8U) | gray;
    }

    return CLKS_TTY_FG;
}

static u32 clks_tty_ansi_parse_params(const char *params, u32 len, u32 *out_values, u32 max_values) {
    u32 count = 0U;
    u32 value = 0U;
    clks_bool has_digit = CLKS_FALSE;
    u32 i;

    if (out_values == CLKS_NULL || max_values == 0U) {
        return 0U;
    }

    if (len == 0U) {
        out_values[0] = 0U;
        return 1U;
    }

    for (i = 0U; i <= len; i++) {
        char ch = (i < len) ? params[i] : ';';

        if (ch >= '0' && ch <= '9') {
            has_digit = CLKS_TRUE;
            value = (value * 10U) + (u32)(ch - '0');
            continue;
        }

        if (ch == ';') {
            if (count < max_values) {
                out_values[count++] = (has_digit == CLKS_TRUE) ? value : 0U;
            }

            value = 0U;
            has_digit = CLKS_FALSE;
            continue;
        }
    }

    if (count == 0U) {
        out_values[0] = 0U;
        return 1U;
    }

    return count;
}

static u32 clks_tty_ansi_param_or_default(const u32 *params, u32 count, u32 index, u32 default_value) {
    if (params == CLKS_NULL || index >= count || params[index] == 0U) {
        return default_value;
    }

    return params[index];
}

static void clks_tty_ansi_clear_line_mode(u32 tty_index, u32 mode) {
    u32 row = clks_tty_cursor_row[tty_index];
    u32 start = 0U;
    u32 end = 0U;
    u32 col;

    if (row >= clks_tty_content_rows()) {
        return;
    }

    if (mode == 0U) {
        start = clks_tty_cursor_col[tty_index];
        end = clks_tty_cols;
    } else if (mode == 1U) {
        start = 0U;
        end = clks_tty_cursor_col[tty_index] + 1U;
    } else {
        start = 0U;
        end = clks_tty_cols;
    }

    if (start >= clks_tty_cols) {
        start = clks_tty_cols;
    }

    if (end > clks_tty_cols) {
        end = clks_tty_cols;
    }

    for (col = start; col < end; col++) {
        clks_tty_cells[tty_index][row][col] = ' ';
        clks_tty_cell_fg[tty_index][row][col] = CLKS_TTY_FG;
        clks_tty_cell_bg[tty_index][row][col] = CLKS_TTY_BG;
        clks_tty_cell_style[tty_index][row][col] = CLKS_TTY_STYLE_NONE;
    }

    if (tty_index == clks_tty_active_index) {
        for (col = start; col < end; col++) {
            clks_tty_draw_cell(tty_index, row, col);
        }
    }
}

static void clks_tty_ansi_clear_screen_mode(u32 tty_index, u32 mode) {
    u32 row;
    u32 col;
    u32 content_rows = clks_tty_content_rows();
    u32 cursor_row = clks_tty_cursor_row[tty_index];
    u32 cursor_col = clks_tty_cursor_col[tty_index];

    if (cursor_row >= content_rows) {
        cursor_row = content_rows - 1U;
    }

    if (cursor_col >= clks_tty_cols) {
        cursor_col = clks_tty_cols - 1U;
    }

    if (mode == 0U) {
        for (row = cursor_row; row < content_rows; row++) {
            u32 start_col = (row == cursor_row) ? cursor_col : 0U;

            for (col = start_col; col < clks_tty_cols; col++) {
                clks_tty_cells[tty_index][row][col] = ' ';
                clks_tty_cell_fg[tty_index][row][col] = CLKS_TTY_FG;
                clks_tty_cell_bg[tty_index][row][col] = CLKS_TTY_BG;
                clks_tty_cell_style[tty_index][row][col] = CLKS_TTY_STYLE_NONE;
            }
        }
    } else if (mode == 1U) {
        for (row = 0U; row <= cursor_row; row++) {
            u32 end_col = (row == cursor_row) ? (cursor_col + 1U) : clks_tty_cols;

            for (col = 0U; col < end_col; col++) {
                clks_tty_cells[tty_index][row][col] = ' ';
                clks_tty_cell_fg[tty_index][row][col] = CLKS_TTY_FG;
                clks_tty_cell_bg[tty_index][row][col] = CLKS_TTY_BG;
                clks_tty_cell_style[tty_index][row][col] = CLKS_TTY_STYLE_NONE;
            }
        }
    } else {
        for (row = 0U; row < content_rows; row++) {
            for (col = 0U; col < clks_tty_cols; col++) {
                clks_tty_cells[tty_index][row][col] = ' ';
                clks_tty_cell_fg[tty_index][row][col] = CLKS_TTY_FG;
                clks_tty_cell_bg[tty_index][row][col] = CLKS_TTY_BG;
                clks_tty_cell_style[tty_index][row][col] = CLKS_TTY_STYLE_NONE;
            }
        }
    }

    if (mode == 3U) {
        clks_tty_scrollback_head[tty_index] = 0U;
        clks_tty_scrollback_count[tty_index] = 0U;
        clks_tty_scrollback_offset[tty_index] = 0U;
    }

    if (tty_index == clks_tty_active_index) {
        clks_tty_redraw_active();
    }
}

static void clks_tty_ansi_apply_sgr_params(u32 tty_index, const char *params, u32 len) {
    u32 values[16];
    u32 count = clks_tty_ansi_parse_params(params, len, values, 16U);
    u32 i;

    if (count == 0U) {
        return;
    }

    for (i = 0U; i < count; i++) {
        u32 code = values[i];

        if (code == 0U) {
            clks_tty_reset_color_state(tty_index);
            continue;
        }

        if (code == 1U) {
            clks_tty_ansi[tty_index].bold = CLKS_TRUE;
            continue;
        }

        if (code == 4U) {
            clks_tty_ansi[tty_index].underline = CLKS_TRUE;
            continue;
        }

        if (code == 7U) {
            clks_tty_ansi[tty_index].inverse = CLKS_TRUE;
            continue;
        }

        if (code == 21U || code == 22U) {
            clks_tty_ansi[tty_index].bold = CLKS_FALSE;
            continue;
        }

        if (code == 24U) {
            clks_tty_ansi[tty_index].underline = CLKS_FALSE;
            continue;
        }

        if (code == 27U) {
            clks_tty_ansi[tty_index].inverse = CLKS_FALSE;
            continue;
        }

        if (code == 39U) {
            clks_tty_current_fg[tty_index] = CLKS_TTY_FG;
            continue;
        }

        if (code == 49U) {
            clks_tty_current_bg[tty_index] = CLKS_TTY_BG;
            continue;
        }

        if (code >= 30U && code <= 37U) {
            u32 idx = code - 30U;

            if (clks_tty_ansi[tty_index].bold == CLKS_TRUE) {
                idx += 8U;
            }

            clks_tty_current_fg[tty_index] = clks_tty_ansi_palette(idx);
            continue;
        }

        if (code >= 90U && code <= 97U) {
            clks_tty_current_fg[tty_index] = clks_tty_ansi_palette((code - 90U) + 8U);
            continue;
        }

        if (code >= 40U && code <= 47U) {
            clks_tty_current_bg[tty_index] = clks_tty_ansi_palette(code - 40U);
            continue;
        }

        if (code >= 100U && code <= 107U) {
            clks_tty_current_bg[tty_index] = clks_tty_ansi_palette((code - 100U) + 8U);
            continue;
        }

        if ((code == 38U || code == 48U) && (i + 1U) < count) {
            u32 mode = values[i + 1U];
            u32 color;

            if (mode == 5U && (i + 2U) < count) {
                color = clks_tty_ansi_color_from_256(values[i + 2U]);

                if (code == 38U) {
                    clks_tty_current_fg[tty_index] = color;
                } else {
                    clks_tty_current_bg[tty_index] = color;
                }

                i += 2U;
                continue;
            }

            if (mode == 2U && (i + 4U) < count) {
                u32 r = clks_tty_ansi_clamp_255(values[i + 2U]);
                u32 g = clks_tty_ansi_clamp_255(values[i + 3U]);
                u32 b = clks_tty_ansi_clamp_255(values[i + 4U]);
                color = (r << 16U) | (g << 8U) | b;

                if (code == 38U) {
                    clks_tty_current_fg[tty_index] = color;
                } else {
                    clks_tty_current_bg[tty_index] = color;
                }

                i += 4U;
                continue;
            }
        }
    }
}

static void clks_tty_ansi_process_csi_final(u32 tty_index, const char *params, u32 len, char final) {
    u32 values[16];
    u32 count = clks_tty_ansi_parse_params(params, len, values, 16U);
    clks_bool private_mode = (len > 0U && params[0] == '?') ? CLKS_TRUE : CLKS_FALSE;

    if (final == 'm') {
        clks_tty_ansi_apply_sgr_params(tty_index, params, len);
        return;
    }

    if (final == 'J') {
        u32 mode = (count == 0U) ? 0U : values[0];
        clks_tty_ansi_clear_screen_mode(tty_index, mode);
        return;
    }

    if (final == 'K') {
        u32 mode = (count == 0U) ? 0U : values[0];
        clks_tty_ansi_clear_line_mode(tty_index, mode);
        return;
    }

    if (final == 'H' || final == 'f') {
        u32 row = clks_tty_ansi_param_or_default(values, count, 0U, 1U);
        u32 col = clks_tty_ansi_param_or_default(values, count, 1U, 1U);

        if (row > 0U) {
            row--;
        }

        if (col > 0U) {
            col--;
        }

        if (row >= clks_tty_content_rows()) {
            row = clks_tty_content_rows() - 1U;
        }

        if (col >= clks_tty_cols) {
            col = clks_tty_cols - 1U;
        }

        clks_tty_cursor_row[tty_index] = row;
        clks_tty_cursor_col[tty_index] = col;
        return;
    }

    if (final == 'A') {
        u32 n = clks_tty_ansi_param_or_default(values, count, 0U, 1U);

        if (n > clks_tty_cursor_row[tty_index]) {
            clks_tty_cursor_row[tty_index] = 0U;
        } else {
            clks_tty_cursor_row[tty_index] -= n;
        }

        return;
    }

    if (final == 'B') {
        u32 n = clks_tty_ansi_param_or_default(values, count, 0U, 1U);
        u32 max_row = clks_tty_content_rows() - 1U;

        if (clks_tty_cursor_row[tty_index] + n > max_row) {
            clks_tty_cursor_row[tty_index] = max_row;
        } else {
            clks_tty_cursor_row[tty_index] += n;
        }

        return;
    }

    if (final == 'C') {
        u32 n = clks_tty_ansi_param_or_default(values, count, 0U, 1U);
        u32 max_col = clks_tty_cols - 1U;

        if (clks_tty_cursor_col[tty_index] + n > max_col) {
            clks_tty_cursor_col[tty_index] = max_col;
        } else {
            clks_tty_cursor_col[tty_index] += n;
        }

        return;
    }

    if (final == 'D') {
        u32 n = clks_tty_ansi_param_or_default(values, count, 0U, 1U);

        if (n > clks_tty_cursor_col[tty_index]) {
            clks_tty_cursor_col[tty_index] = 0U;
        } else {
            clks_tty_cursor_col[tty_index] -= n;
        }

        return;
    }

    if (final == 'E' || final == 'F') {
        u32 n = clks_tty_ansi_param_or_default(values, count, 0U, 1U);

        if (final == 'E') {
            u32 max_row = clks_tty_content_rows() - 1U;

            if (clks_tty_cursor_row[tty_index] + n > max_row) {
                clks_tty_cursor_row[tty_index] = max_row;
            } else {
                clks_tty_cursor_row[tty_index] += n;
            }
        } else {
            if (n > clks_tty_cursor_row[tty_index]) {
                clks_tty_cursor_row[tty_index] = 0U;
            } else {
                clks_tty_cursor_row[tty_index] -= n;
            }
        }

        clks_tty_cursor_col[tty_index] = 0U;
        return;
    }

    if (final == 'G') {
        u32 col = clks_tty_ansi_param_or_default(values, count, 0U, 1U);

        if (col > 0U) {
            col--;
        }

        if (col >= clks_tty_cols) {
            col = clks_tty_cols - 1U;
        }

        clks_tty_cursor_col[tty_index] = col;
        return;
    }

    if (final == 'd') {
        u32 row = clks_tty_ansi_param_or_default(values, count, 0U, 1U);

        if (row > 0U) {
            row--;
        }

        if (row >= clks_tty_content_rows()) {
            row = clks_tty_content_rows() - 1U;
        }

        clks_tty_cursor_row[tty_index] = row;
        return;
    }

    if (final == 's') {
        clks_tty_ansi[tty_index].saved_row = clks_tty_cursor_row[tty_index];
        clks_tty_ansi[tty_index].saved_col = clks_tty_cursor_col[tty_index];
        return;
    }

    if (final == 'u') {
        u32 row = clks_tty_ansi[tty_index].saved_row;
        u32 col = clks_tty_ansi[tty_index].saved_col;

        if (row >= clks_tty_content_rows()) {
            row = clks_tty_content_rows() - 1U;
        }

        if (col >= clks_tty_cols) {
            col = clks_tty_cols - 1U;
        }

        clks_tty_cursor_row[tty_index] = row;
        clks_tty_cursor_col[tty_index] = col;
        return;
    }

    if ((final == 'h' || final == 'l') && private_mode == CLKS_TRUE) {
        u32 mode = (count == 0U) ? 0U : values[0];

        if (mode == 25U) {
            if (final == 'h') {
                clks_tty_blink_enabled = CLKS_TRUE;
                if (tty_index == clks_tty_active_index) {
                    clks_tty_draw_cursor();
                }
            } else {
                clks_tty_blink_enabled = CLKS_FALSE;
                if (tty_index == clks_tty_active_index) {
                    clks_tty_hide_cursor();
                }
            }
        }

        return;
    }
}

static clks_bool clks_tty_ansi_process_byte(u32 tty_index, char ch) {
    clks_tty_ansi_state *state = &clks_tty_ansi[tty_index];

    if (state->in_escape == CLKS_FALSE) {
        if ((u8)ch == 0x1BU) {
            state->in_escape = CLKS_TRUE;
            state->saw_csi = CLKS_FALSE;
            state->len = 0U;
            state->params[0] = '\0';
            return CLKS_TRUE;
        }

        return CLKS_FALSE;
    }

    if (state->saw_csi == CLKS_FALSE) {
        if (ch == '[') {
            state->saw_csi = CLKS_TRUE;
            return CLKS_TRUE;
        }

        if (ch == '7') {
            state->saved_row = clks_tty_cursor_row[tty_index];
            state->saved_col = clks_tty_cursor_col[tty_index];
            clks_tty_reset_ansi_state(tty_index);
            return CLKS_TRUE;
        }

        if (ch == '8') {
            u32 row = state->saved_row;
            u32 col = state->saved_col;

            if (row >= clks_tty_content_rows()) {
                row = clks_tty_content_rows() - 1U;
            }

            if (col >= clks_tty_cols) {
                col = clks_tty_cols - 1U;
            }

            clks_tty_cursor_row[tty_index] = row;
            clks_tty_cursor_col[tty_index] = col;
            clks_tty_reset_ansi_state(tty_index);
            return CLKS_TRUE;
        }

        clks_tty_reset_ansi_state(tty_index);
        return CLKS_TRUE;
    }

    if ((ch >= '0' && ch <= '9') || ch == ';' || ch == '?') {
        if (state->len < CLKS_TTY_ANSI_MAX_LEN) {
            state->params[state->len++] = ch;
            state->params[state->len] = '\0';
        } else {
            clks_tty_reset_ansi_state(tty_index);
        }

        return CLKS_TRUE;
    }

    clks_tty_ansi_process_csi_final(tty_index, state->params, state->len, ch);
    clks_tty_reset_ansi_state(tty_index);
    return CLKS_TRUE;
}
void clks_tty_init(void) {
    struct clks_framebuffer_info info;
    u32 tty;

    if (clks_fb_ready() == CLKS_FALSE) {
        clks_tty_is_ready = CLKS_FALSE;
        return;
    }

    info = clks_fb_info();
    clks_tty_cell_width = clks_fb_cell_width();
    clks_tty_cell_height = clks_fb_cell_height();

    if (clks_tty_cell_width == 0U) {
        clks_tty_cell_width = 8U;
    }

    if (clks_tty_cell_height == 0U) {
        clks_tty_cell_height = 8U;
    }

    clks_tty_rows = info.height / clks_tty_cell_height;
    clks_tty_cols = info.width / clks_tty_cell_width;

    if (clks_tty_rows > CLKS_TTY_MAX_ROWS) {
        clks_tty_rows = CLKS_TTY_MAX_ROWS;
    }

    if (clks_tty_cols > CLKS_TTY_MAX_COLS) {
        clks_tty_cols = CLKS_TTY_MAX_COLS;
    }

    if (clks_tty_rows == 0U || clks_tty_cols == 0U) {
        clks_tty_is_ready = CLKS_FALSE;
        return;
    }

    for (tty = 0U; tty < CLKS_TTY_COUNT; tty++) {
        clks_tty_cursor_row[tty] = 0U;
        clks_tty_cursor_col[tty] = 0U;
        clks_tty_reset_color_state(tty);
        clks_tty_reset_ansi_state(tty);
        clks_tty_clear_tty(tty);
    }

    clks_tty_active_index = 0U;
    clks_tty_is_ready = CLKS_TRUE;
    clks_tty_cursor_visible = CLKS_FALSE;
    clks_tty_blink_enabled = CLKS_TRUE;
    clks_tty_reset_blink_timer();
    clks_tty_redraw_active();
}

void clks_tty_write_char(char ch) {
    u32 tty_index;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    clks_tty_hide_cursor();

    tty_index = clks_tty_active_index;

    if (clks_tty_scrollback_is_active(tty_index) == CLKS_TRUE) {
        clks_tty_scrollback_follow_tail(tty_index);
        clks_tty_redraw_active();
    }

    if (clks_tty_ansi_process_byte(tty_index, ch) == CLKS_FALSE) {
        clks_tty_put_char_raw(tty_index, ch);
    }

    clks_tty_draw_cursor();
    clks_tty_draw_status_bar();
    clks_tty_reset_blink_timer();
}

void clks_tty_write_n(const char *text, usize len) {
    usize i = 0U;
    u32 tty_index;

    if (clks_tty_is_ready == CLKS_FALSE || text == CLKS_NULL || len == 0U) {
        return;
    }

    clks_tty_hide_cursor();
    tty_index = clks_tty_active_index;

    if (clks_tty_scrollback_is_active(tty_index) == CLKS_TRUE) {
        clks_tty_scrollback_follow_tail(tty_index);
        clks_tty_redraw_active();
    }

    while (i < len) {
        if (clks_tty_ansi_process_byte(tty_index, text[i]) == CLKS_FALSE) {
            clks_tty_put_char_raw(tty_index, text[i]);
        }

        i++;
    }

    clks_tty_draw_cursor();
    clks_tty_draw_status_bar();
    clks_tty_reset_blink_timer();
}

void clks_tty_write(const char *text) {
    if (text == CLKS_NULL) {
        return;
    }

    clks_tty_write_n(text, clks_strlen(text));
}

void clks_tty_switch(u32 tty_index) {
    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    if (tty_index >= CLKS_TTY_COUNT) {
        return;
    }

    clks_tty_hide_cursor();
    clks_tty_active_index = tty_index;
    clks_tty_cursor_visible = CLKS_FALSE;
    clks_tty_redraw_active();
    clks_tty_reset_blink_timer();
}

void clks_tty_tick(u64 tick) {
    if (clks_tty_is_ready == CLKS_FALSE || clks_tty_blink_enabled == CLKS_FALSE) {
        return;
    }

    if (clks_tty_active_index == CLKS_TTY_DESKTOP_INDEX) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    if (clks_tty_scrollback_is_active(clks_tty_active_index) == CLKS_TRUE) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    if (clks_tty_blink_last_tick == CLKS_TTY_BLINK_TICK_UNSET) {
        clks_tty_blink_last_tick = tick;

        if (clks_tty_cursor_visible == CLKS_FALSE) {
            clks_tty_draw_cursor();
        }

        return;
    }

    if (tick < clks_tty_blink_last_tick) {
        clks_tty_blink_last_tick = tick;
        return;
    }

    if ((tick - clks_tty_blink_last_tick) < CLKS_TTY_CURSOR_BLINK_INTERVAL_TICKS) {
        return;
    }

    clks_tty_blink_last_tick = tick;

    if (clks_tty_cursor_visible == CLKS_TRUE) {
        clks_tty_hide_cursor();
    } else {
        clks_tty_draw_cursor();
    }
}

void clks_tty_scrollback_page_up(void) {
    u32 tty_index;
    u32 max_offset;
    u32 next_offset;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    tty_index = clks_tty_active_index;

    if (tty_index == CLKS_TTY_DESKTOP_INDEX) {
        return;
    }

    max_offset = clks_tty_scrollback_max_offset(tty_index);

    if (max_offset == 0U) {
        return;
    }

    clks_tty_hide_cursor();
    next_offset = clks_tty_scrollback_clamped_offset(tty_index) + clks_tty_content_rows();

    if (next_offset > max_offset) {
        next_offset = max_offset;
    }

    if (next_offset != clks_tty_scrollback_offset[tty_index]) {
        clks_tty_scrollback_offset[tty_index] = next_offset;
        clks_tty_redraw_active();
        clks_tty_reset_blink_timer();
    }
}

void clks_tty_scrollback_page_down(void) {
    u32 tty_index;
    u32 current_offset;
    u32 next_offset;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    tty_index = clks_tty_active_index;

    if (tty_index == CLKS_TTY_DESKTOP_INDEX) {
        return;
    }

    current_offset = clks_tty_scrollback_clamped_offset(tty_index);

    if (current_offset == 0U) {
        return;
    }

    clks_tty_hide_cursor();

    if (current_offset > clks_tty_content_rows()) {
        next_offset = current_offset - clks_tty_content_rows();
    } else {
        next_offset = 0U;
    }

    if (next_offset != clks_tty_scrollback_offset[tty_index]) {
        clks_tty_scrollback_offset[tty_index] = next_offset;
        clks_tty_redraw_active();
        clks_tty_reset_blink_timer();
    }
}


u32 clks_tty_active(void) {
    return clks_tty_active_index;
}

u32 clks_tty_count(void) {
    return CLKS_TTY_COUNT;
}

clks_bool clks_tty_ready(void) {
    return clks_tty_is_ready;
}

