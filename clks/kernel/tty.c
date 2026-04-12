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

static char clks_tty_cells[CLKS_TTY_COUNT][CLKS_TTY_MAX_ROWS][CLKS_TTY_MAX_COLS];
static u32 clks_tty_cursor_row[CLKS_TTY_COUNT];
static u32 clks_tty_cursor_col[CLKS_TTY_COUNT];

static u32 clks_tty_rows = 0;
static u32 clks_tty_cols = 0;
static u32 clks_tty_active_index = 0;
static u32 clks_tty_cell_width = 8U;
static u32 clks_tty_cell_height = 8U;
static clks_bool clks_tty_is_ready = CLKS_FALSE;
static clks_bool clks_tty_cursor_visible = CLKS_FALSE;
static clks_bool clks_tty_blink_enabled = CLKS_TRUE;
static u64 clks_tty_blink_last_tick = CLKS_TTY_BLINK_TICK_UNSET;

static void clks_tty_fill_row(u32 tty_index, u32 row, char ch) {
    u32 col;

    for (col = 0; col < clks_tty_cols; col++) {
        clks_tty_cells[tty_index][row][col] = ch;
    }
}

static void clks_tty_draw_cell_with_colors(u32 row, u32 col, char ch, u32 fg, u32 bg) {
    clks_fb_draw_char(col * clks_tty_cell_width, row * clks_tty_cell_height, ch, fg, bg);
}

static void clks_tty_draw_cell(u32 row, u32 col, char ch) {
    clks_tty_draw_cell_with_colors(row, col, ch, CLKS_TTY_FG, CLKS_TTY_BG);
}

static void clks_tty_reset_blink_timer(void) {
    clks_tty_blink_last_tick = CLKS_TTY_BLINK_TICK_UNSET;
}

static void clks_tty_hide_cursor(void) {
    u32 row;
    u32 col;

    if (clks_tty_is_ready == CLKS_FALSE || clks_tty_cursor_visible == CLKS_FALSE) {
        return;
    }

    row = clks_tty_cursor_row[clks_tty_active_index];
    col = clks_tty_cursor_col[clks_tty_active_index];

    if (row < clks_tty_rows && col < clks_tty_cols) {
        clks_tty_draw_cell(row, col, clks_tty_cells[clks_tty_active_index][row][col]);
    }

    clks_tty_cursor_visible = CLKS_FALSE;
}

static void clks_tty_draw_cursor(void) {
    u32 row;
    u32 col;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    if (clks_tty_active_index == CLKS_TTY_DESKTOP_INDEX) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    row = clks_tty_cursor_row[clks_tty_active_index];
    col = clks_tty_cursor_col[clks_tty_active_index];

    if (row >= clks_tty_rows || col >= clks_tty_cols) {
        clks_tty_cursor_visible = CLKS_FALSE;
        return;
    }

    clks_tty_draw_cell_with_colors(
        row,
        col,
        clks_tty_cells[clks_tty_active_index][row][col],
        CLKS_TTY_BG,
        CLKS_TTY_FG
    );

    clks_tty_cursor_visible = CLKS_TRUE;
}

static void clks_tty_redraw_active(void) {
    u32 row;
    u32 col;

    clks_fb_clear(CLKS_TTY_BG);
    clks_tty_cursor_visible = CLKS_FALSE;

    for (row = 0; row < clks_tty_rows; row++) {
        for (col = 0; col < clks_tty_cols; col++) {
            clks_tty_draw_cell(row, col, clks_tty_cells[clks_tty_active_index][row][col]);
        }
    }

    clks_tty_draw_cursor();
}

static void clks_tty_scroll_up(u32 tty_index) {
    u32 row;

    for (row = 1; row < clks_tty_rows; row++) {
        clks_memcpy(
            clks_tty_cells[tty_index][row - 1],
            clks_tty_cells[tty_index][row],
            clks_tty_cols
        );
    }

    clks_tty_fill_row(tty_index, clks_tty_rows - 1, ' ');

    if (tty_index == clks_tty_active_index) {
        u32 col;

        clks_fb_scroll_up(clks_tty_cell_height, CLKS_TTY_BG);

        for (col = 0U; col < clks_tty_cols; col++) {
            clks_tty_draw_cell(clks_tty_rows - 1U, col, clks_tty_cells[tty_index][clks_tty_rows - 1U][col]);
        }
    }
}

static void clks_tty_put_visible(u32 tty_index, u32 row, u32 col, char ch) {
    clks_tty_cells[tty_index][row][col] = ch;

    if (tty_index == clks_tty_active_index) {
        clks_tty_draw_cell(row, col, ch);
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

        if (clks_tty_cursor_row[tty_index] >= clks_tty_rows) {
            clks_tty_scroll_up(tty_index);
            clks_tty_cursor_row[tty_index] = clks_tty_rows - 1U;
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

        if (clks_tty_cursor_row[tty_index] >= clks_tty_rows) {
            clks_tty_scroll_up(tty_index);
            clks_tty_cursor_row[tty_index] = clks_tty_rows - 1U;
        }
    }
}

void clks_tty_init(void) {
    struct clks_framebuffer_info info;
    u32 tty;
    u32 row;

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

    for (tty = 0; tty < CLKS_TTY_COUNT; tty++) {
        clks_tty_cursor_row[tty] = 0;
        clks_tty_cursor_col[tty] = 0;

        for (row = 0; row < clks_tty_rows; row++) {
            clks_tty_fill_row(tty, row, ' ');
        }
    }

    clks_tty_active_index = 0;
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
    clks_tty_put_char_raw(tty_index, ch);

    clks_tty_draw_cursor();
    clks_tty_reset_blink_timer();
}

void clks_tty_write(const char *text) {
    usize i = 0U;
    u32 tty_index;

    if (clks_tty_is_ready == CLKS_FALSE || text == CLKS_NULL) {
        return;
    }

    clks_tty_hide_cursor();
    tty_index = clks_tty_active_index;

    while (text[i] != '\0') {
        clks_tty_put_char_raw(tty_index, text[i]);
        i++;
    }

    clks_tty_draw_cursor();
    clks_tty_reset_blink_timer();
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

u32 clks_tty_active(void) {
    return clks_tty_active_index;
}

u32 clks_tty_count(void) {
    return CLKS_TTY_COUNT;
}

clks_bool clks_tty_ready(void) {
    return clks_tty_is_ready;
}
