#include <clks/framebuffer.h>
#include <clks/string.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_TTY_COUNT 4
#define CLKS_TTY_MAX_ROWS 128
#define CLKS_TTY_MAX_COLS 256

#define CLKS_TTY_FG 0x00E6E6E6U
#define CLKS_TTY_BG 0x00101010U

static char clks_tty_cells[CLKS_TTY_COUNT][CLKS_TTY_MAX_ROWS][CLKS_TTY_MAX_COLS];
static u32 clks_tty_cursor_row[CLKS_TTY_COUNT];
static u32 clks_tty_cursor_col[CLKS_TTY_COUNT];

static u32 clks_tty_rows = 0;
static u32 clks_tty_cols = 0;
static u32 clks_tty_active_index = 0;
static clks_bool clks_tty_is_ready = CLKS_FALSE;

static void clks_tty_fill_row(u32 tty_index, u32 row, char ch) {
    u32 col;

    for (col = 0; col < clks_tty_cols; col++) {
        clks_tty_cells[tty_index][row][col] = ch;
    }
}

static void clks_tty_draw_cell(u32 row, u32 col, char ch) {
    clks_fb_draw_char(col * 8U, row * 8U, ch, CLKS_TTY_FG, CLKS_TTY_BG);
}

static void clks_tty_redraw_active(void) {
    u32 row;
    u32 col;

    clks_fb_clear(CLKS_TTY_BG);

    for (row = 0; row < clks_tty_rows; row++) {
        for (col = 0; col < clks_tty_cols; col++) {
            clks_tty_draw_cell(row, col, clks_tty_cells[clks_tty_active_index][row][col]);
        }
    }
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
        clks_tty_redraw_active();
    }
}

static void clks_tty_put_visible(u32 tty_index, u32 row, u32 col, char ch) {
    clks_tty_cells[tty_index][row][col] = ch;

    if (tty_index == clks_tty_active_index) {
        clks_tty_draw_cell(row, col, ch);
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
    clks_tty_rows = info.height / 8U;
    clks_tty_cols = info.width / 8U;

    if (clks_tty_rows > CLKS_TTY_MAX_ROWS) {
        clks_tty_rows = CLKS_TTY_MAX_ROWS;
    }

    if (clks_tty_cols > CLKS_TTY_MAX_COLS) {
        clks_tty_cols = CLKS_TTY_MAX_COLS;
    }

    if (clks_tty_rows == 0 || clks_tty_cols == 0) {
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
    clks_tty_redraw_active();
}

void clks_tty_write_char(char ch) {
    u32 tty_index;
    u32 row;
    u32 col;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    tty_index = clks_tty_active_index;
    row = clks_tty_cursor_row[tty_index];
    col = clks_tty_cursor_col[tty_index];

    if (ch == '\r') {
        clks_tty_cursor_col[tty_index] = 0;
        return;
    }

    if (ch == '\n') {
        clks_tty_cursor_col[tty_index] = 0;
        clks_tty_cursor_row[tty_index]++;

        if (clks_tty_cursor_row[tty_index] >= clks_tty_rows) {
            clks_tty_scroll_up(tty_index);
            clks_tty_cursor_row[tty_index] = clks_tty_rows - 1;
        }

        return;
    }

    if (ch == '\t') {
        clks_tty_write_char(' ');
        clks_tty_write_char(' ');
        clks_tty_write_char(' ');
        clks_tty_write_char(' ');
        return;
    }

    clks_tty_put_visible(tty_index, row, col, ch);
    clks_tty_cursor_col[tty_index]++;

    if (clks_tty_cursor_col[tty_index] >= clks_tty_cols) {
        clks_tty_cursor_col[tty_index] = 0;
        clks_tty_cursor_row[tty_index]++;

        if (clks_tty_cursor_row[tty_index] >= clks_tty_rows) {
            clks_tty_scroll_up(tty_index);
            clks_tty_cursor_row[tty_index] = clks_tty_rows - 1;
        }
    }
}

void clks_tty_write(const char *text) {
    usize i = 0;

    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    while (text[i] != '\0') {
        clks_tty_write_char(text[i]);
        i++;
    }
}

void clks_tty_switch(u32 tty_index) {
    if (clks_tty_is_ready == CLKS_FALSE) {
        return;
    }

    if (tty_index >= CLKS_TTY_COUNT) {
        return;
    }

    clks_tty_active_index = tty_index;
    clks_tty_redraw_active();
}

u32 clks_tty_active(void) {
    return clks_tty_active_index;
}