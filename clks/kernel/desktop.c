#include <clks/desktop.h>
#include <clks/framebuffer.h>
#include <clks/log.h>
#include <clks/mouse.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_DESKTOP_TTY_INDEX       1U

#define CLKS_DESKTOP_BG_COLOR        0x001B2430U
#define CLKS_DESKTOP_TOPBAR_COLOR    0x00293447U
#define CLKS_DESKTOP_DOCK_COLOR      0x00232C3AU
#define CLKS_DESKTOP_WINDOW_COLOR    0x00313E52U
#define CLKS_DESKTOP_TITLE_COLOR     0x003B4A61U
#define CLKS_DESKTOP_TEXT_FG         0x00E6EDF7U
#define CLKS_DESKTOP_TEXT_BG         0x003B4A61U

#define CLKS_DESKTOP_CURSOR_FILL     0x00F5F8FFU
#define CLKS_DESKTOP_CURSOR_OUTLINE  0x00101010U
#define CLKS_DESKTOP_CURSOR_ACTIVE   0x00FFCE6EU
#define CLKS_DESKTOP_CURSOR_W        16U
#define CLKS_DESKTOP_CURSOR_H        16U

struct clks_desktop_layout {
    u32 width;
    u32 height;
    u32 topbar_h;
    u32 dock_w;
    u32 win_x;
    u32 win_y;
    u32 win_w;
    u32 win_h;
    u32 win_title_h;
};

static struct clks_desktop_layout clks_desktop = {
    .width = 0U,
    .height = 0U,
    .topbar_h = 32U,
    .dock_w = 72U,
    .win_x = 0U,
    .win_y = 0U,
    .win_w = 0U,
    .win_h = 0U,
    .win_title_h = 28U,
};

static clks_bool clks_desktop_ready_flag = CLKS_FALSE;
static clks_bool clks_desktop_active_last = CLKS_FALSE;
static clks_bool clks_desktop_scene_drawn = CLKS_FALSE;
static clks_bool clks_desktop_cursor_drawn = CLKS_FALSE;
static i32 clks_desktop_last_mouse_x = -1;
static i32 clks_desktop_last_mouse_y = -1;
static u8 clks_desktop_last_buttons = 0U;
static clks_bool clks_desktop_last_ready = CLKS_FALSE;
static u32 clks_desktop_cursor_under[CLKS_DESKTOP_CURSOR_W * CLKS_DESKTOP_CURSOR_H];

static void clks_desktop_draw_text(u32 x, u32 y, const char *text, u32 fg, u32 bg) {
    u32 step;
    usize i = 0U;

    if (text == CLKS_NULL) {
        return;
    }

    step = clks_fb_cell_width();

    if (step == 0U) {
        step = 8U;
    }

    while (text[i] != '\0') {
        clks_fb_draw_char(x + ((u32)i * step), y, text[i], fg, bg);
        i++;
    }
}

static clks_bool clks_desktop_in_bounds(i32 x, i32 y) {
    if (x < 0 || y < 0) {
        return CLKS_FALSE;
    }

    if ((u32)x >= clks_desktop.width || (u32)y >= clks_desktop.height) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static void clks_desktop_compute_layout(void) {
    struct clks_framebuffer_info info = clks_fb_info();
    u32 right_margin;
    u32 bottom_margin;

    clks_desktop.width = info.width;
    clks_desktop.height = info.height;

    if (clks_desktop.topbar_h >= clks_desktop.height) {
        clks_desktop.topbar_h = clks_desktop.height;
    }

    if (clks_desktop.dock_w >= clks_desktop.width) {
        clks_desktop.dock_w = clks_desktop.width;
    }

    clks_desktop.win_x = clks_desktop.width / 6U;
    clks_desktop.win_y = clks_desktop.height / 6U;

    if (clks_desktop.win_x < 96U) {
        clks_desktop.win_x = 96U;
    }

    if (clks_desktop.win_y < 72U) {
        clks_desktop.win_y = 72U;
    }

    if (clks_desktop.win_x >= clks_desktop.width) {
        clks_desktop.win_x = clks_desktop.width > 0U ? (clks_desktop.width - 1U) : 0U;
    }

    if (clks_desktop.win_y >= clks_desktop.height) {
        clks_desktop.win_y = clks_desktop.height > 0U ? (clks_desktop.height - 1U) : 0U;
    }

    right_margin = clks_desktop.width / 10U;
    bottom_margin = clks_desktop.height / 8U;

    if (right_margin < 48U) {
        right_margin = 48U;
    }

    if (bottom_margin < 48U) {
        bottom_margin = 48U;
    }

    if (clks_desktop.width > clks_desktop.win_x + right_margin) {
        clks_desktop.win_w = clks_desktop.width - clks_desktop.win_x - right_margin;
    } else {
        clks_desktop.win_w = clks_desktop.width - clks_desktop.win_x;
    }

    if (clks_desktop.height > clks_desktop.win_y + bottom_margin) {
        clks_desktop.win_h = clks_desktop.height - clks_desktop.win_y - bottom_margin;
    } else {
        clks_desktop.win_h = clks_desktop.height - clks_desktop.win_y;
    }

    if (clks_desktop.win_w < 220U) {
        clks_desktop.win_w = (clks_desktop.width > (clks_desktop.win_x + 20U))
                                 ? (clks_desktop.width - clks_desktop.win_x - 20U)
                                 : (clks_desktop.width - clks_desktop.win_x);
    }

    if (clks_desktop.win_h < 140U) {
        clks_desktop.win_h = (clks_desktop.height > (clks_desktop.win_y + 20U))
                                 ? (clks_desktop.height - clks_desktop.win_y - 20U)
                                 : (clks_desktop.height - clks_desktop.win_y);
    }

    if (clks_desktop.win_w > (clks_desktop.width - clks_desktop.win_x)) {
        clks_desktop.win_w = clks_desktop.width - clks_desktop.win_x;
    }

    if (clks_desktop.win_h > (clks_desktop.height - clks_desktop.win_y)) {
        clks_desktop.win_h = clks_desktop.height - clks_desktop.win_y;
    }

    if (clks_desktop.win_title_h >= clks_desktop.win_h) {
        clks_desktop.win_title_h = clks_desktop.win_h;
    }
}

static void clks_desktop_draw_status_widgets(const struct clks_mouse_state *mouse) {
    u32 ready_x = 0U;
    u32 left_x = 0U;

    if (mouse == CLKS_NULL) {
        return;
    }

    if (clks_desktop.width > 28U) {
        ready_x = clks_desktop.width - 28U;
    }

    if (clks_desktop.width > 46U) {
        left_x = clks_desktop.width - 46U;
    }

    if (mouse->ready == CLKS_TRUE) {
        clks_fb_fill_rect(ready_x, 10U, 10U, 10U, 0x006FE18BU);
    } else {
        clks_fb_fill_rect(ready_x, 10U, 10U, 10U, 0x00E06A6AU);
    }

    if ((mouse->buttons & CLKS_MOUSE_BTN_LEFT) != 0U) {
        clks_fb_fill_rect(left_x, 10U, 10U, 10U, 0x00FFCE6EU);
    } else {
        clks_fb_fill_rect(left_x, 10U, 10U, 10U, 0x004A5568U);
    }
}

static void clks_desktop_draw_static_scene(const struct clks_mouse_state *mouse) {
    clks_fb_clear(CLKS_DESKTOP_BG_COLOR);

    if (clks_desktop.topbar_h > 0U) {
        clks_fb_fill_rect(0U, 0U, clks_desktop.width, clks_desktop.topbar_h, CLKS_DESKTOP_TOPBAR_COLOR);
    }

    if (clks_desktop.height > clks_desktop.topbar_h) {
        clks_fb_fill_rect(0U, clks_desktop.topbar_h, clks_desktop.dock_w,
                          clks_desktop.height - clks_desktop.topbar_h, CLKS_DESKTOP_DOCK_COLOR);
    }

    clks_fb_fill_rect(clks_desktop.win_x, clks_desktop.win_y, clks_desktop.win_w, clks_desktop.win_h,
                      CLKS_DESKTOP_WINDOW_COLOR);
    clks_fb_fill_rect(clks_desktop.win_x, clks_desktop.win_y, clks_desktop.win_w, clks_desktop.win_title_h,
                      CLKS_DESKTOP_TITLE_COLOR);

    clks_desktop_draw_text(12U, 6U, "CLeonOS Desktop TTY2", CLKS_DESKTOP_TEXT_FG, CLKS_DESKTOP_TOPBAR_COLOR);
    clks_desktop_draw_text(clks_desktop.win_x + 12U, clks_desktop.win_y + 6U, "Mouse Input Ready",
                           CLKS_DESKTOP_TEXT_FG, CLKS_DESKTOP_TITLE_COLOR);
    clks_desktop_draw_text(clks_desktop.win_x + 16U, clks_desktop.win_y + clks_desktop.win_title_h + 16U,
                           "Stage25: Alt+F2 desktop, Alt+F1 shell", CLKS_DESKTOP_TEXT_FG, CLKS_DESKTOP_WINDOW_COLOR);

    clks_desktop_draw_status_widgets(mouse);
    clks_desktop_scene_drawn = CLKS_TRUE;
}

static clks_bool clks_desktop_cursor_pixel(u32 lx, u32 ly, u8 buttons, u32 *out_color) {
    u32 fill = CLKS_DESKTOP_CURSOR_FILL;

    if (out_color == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if ((buttons & CLKS_MOUSE_BTN_LEFT) != 0U) {
        fill = CLKS_DESKTOP_CURSOR_ACTIVE;
    }

    if (ly < 12U) {
        u32 span = (ly / 2U) + 1U;

        if (lx < span) {
            clks_bool border = (lx == 0U || (lx + 1U) == span || ly == 11U) ? CLKS_TRUE : CLKS_FALSE;
            *out_color = (border == CLKS_TRUE) ? CLKS_DESKTOP_CURSOR_OUTLINE : fill;
            return CLKS_TRUE;
        }
    }

    if (ly >= 8U && ly < 16U && lx >= 2U && lx < 5U) {
        clks_bool border = (lx == 2U || lx == 4U || ly == 15U) ? CLKS_TRUE : CLKS_FALSE;
        *out_color = (border == CLKS_TRUE) ? CLKS_DESKTOP_CURSOR_OUTLINE : fill;
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

static void clks_desktop_capture_cursor_under(i32 x, i32 y) {
    u32 ly;

    for (ly = 0U; ly < CLKS_DESKTOP_CURSOR_H; ly++) {
        u32 lx;

        for (lx = 0U; lx < CLKS_DESKTOP_CURSOR_W; lx++) {
            i32 gx = x + (i32)lx;
            i32 gy = y + (i32)ly;
            usize idx = ((usize)ly * (usize)CLKS_DESKTOP_CURSOR_W) + (usize)lx;
            u32 pixel = CLKS_DESKTOP_BG_COLOR;

            if (clks_desktop_in_bounds(gx, gy) == CLKS_TRUE) {
                (void)clks_fb_read_pixel((u32)gx, (u32)gy, &pixel);
            }

            clks_desktop_cursor_under[idx] = pixel;
        }
    }
}

static void clks_desktop_restore_cursor_under(void) {
    u32 ly;

    if (clks_desktop_cursor_drawn == CLKS_FALSE) {
        return;
    }

    for (ly = 0U; ly < CLKS_DESKTOP_CURSOR_H; ly++) {
        u32 lx;

        for (lx = 0U; lx < CLKS_DESKTOP_CURSOR_W; lx++) {
            i32 gx = clks_desktop_last_mouse_x + (i32)lx;
            i32 gy = clks_desktop_last_mouse_y + (i32)ly;
            usize idx = ((usize)ly * (usize)CLKS_DESKTOP_CURSOR_W) + (usize)lx;

            if (clks_desktop_in_bounds(gx, gy) == CLKS_TRUE) {
                clks_fb_draw_pixel((u32)gx, (u32)gy, clks_desktop_cursor_under[idx]);
            }
        }
    }

    clks_desktop_cursor_drawn = CLKS_FALSE;
}

static void clks_desktop_draw_cursor(i32 x, i32 y, u8 buttons) {
    u32 ly;

    for (ly = 0U; ly < CLKS_DESKTOP_CURSOR_H; ly++) {
        u32 lx;

        for (lx = 0U; lx < CLKS_DESKTOP_CURSOR_W; lx++) {
            i32 gx = x + (i32)lx;
            i32 gy = y + (i32)ly;
            u32 color = 0U;

            if (clks_desktop_cursor_pixel(lx, ly, buttons, &color) == CLKS_FALSE) {
                continue;
            }

            if (clks_desktop_in_bounds(gx, gy) == CLKS_TRUE) {
                clks_fb_draw_pixel((u32)gx, (u32)gy, color);
            }
        }
    }

    clks_desktop_cursor_drawn = CLKS_TRUE;
}

static void clks_desktop_present_cursor(const struct clks_mouse_state *mouse) {
    if (mouse == CLKS_NULL) {
        return;
    }

    clks_desktop_restore_cursor_under();
    clks_desktop_capture_cursor_under(mouse->x, mouse->y);
    clks_desktop_draw_cursor(mouse->x, mouse->y, mouse->buttons);

    clks_desktop_last_mouse_x = mouse->x;
    clks_desktop_last_mouse_y = mouse->y;
    clks_desktop_last_buttons = mouse->buttons;
    clks_desktop_last_ready = mouse->ready;
}

void clks_desktop_init(void) {
    if (clks_fb_ready() == CLKS_FALSE) {
        clks_desktop_ready_flag = CLKS_FALSE;
        clks_log(CLKS_LOG_WARN, "DESK", "FRAMEBUFFER NOT READY; DESKTOP DISABLED");
        return;
    }

    clks_desktop_compute_layout();
    clks_desktop_ready_flag = CLKS_TRUE;
    clks_desktop_active_last = CLKS_FALSE;
    clks_desktop_scene_drawn = CLKS_FALSE;
    clks_desktop_cursor_drawn = CLKS_FALSE;
    clks_desktop_last_mouse_x = -1;
    clks_desktop_last_mouse_y = -1;
    clks_desktop_last_buttons = 0U;
    clks_desktop_last_ready = CLKS_FALSE;

    clks_log(CLKS_LOG_INFO, "DESK", "TTY2 DESKTOP ONLINE");
    clks_log(CLKS_LOG_INFO, "DESK", "MOUSE-FIRST MODE ENABLED");
}

void clks_desktop_tick(u64 tick) {
    struct clks_mouse_state mouse = {0, 0, 0U, 0ULL, CLKS_FALSE};

    (void)tick;

    if (clks_desktop_ready_flag == CLKS_FALSE) {
        return;
    }

    if (clks_tty_active() != CLKS_DESKTOP_TTY_INDEX) {
        clks_desktop_active_last = CLKS_FALSE;
        clks_desktop_scene_drawn = CLKS_FALSE;
        clks_desktop_cursor_drawn = CLKS_FALSE;
        return;
    }

    clks_mouse_snapshot(&mouse);

    if (clks_desktop_active_last == CLKS_FALSE || clks_desktop_scene_drawn == CLKS_FALSE) {
        clks_desktop_compute_layout();
        clks_desktop_draw_static_scene(&mouse);
        clks_desktop_present_cursor(&mouse);
        clks_desktop_active_last = CLKS_TRUE;
        return;
    }

    if (mouse.ready != clks_desktop_last_ready || mouse.buttons != clks_desktop_last_buttons) {
        clks_desktop_draw_status_widgets(&mouse);
    }

    if (mouse.x != clks_desktop_last_mouse_x || mouse.y != clks_desktop_last_mouse_y ||
        mouse.buttons != clks_desktop_last_buttons) {
        clks_desktop_present_cursor(&mouse);
    } else {
        clks_desktop_last_ready = mouse.ready;
    }

    clks_desktop_active_last = CLKS_TRUE;
}

clks_bool clks_desktop_ready(void) {
    return clks_desktop_ready_flag;
}
