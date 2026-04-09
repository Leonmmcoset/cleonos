#include <clks/framebuffer.h>
#include <clks/string.h>
#include <clks/types.h>

#include "font8x8.h"

struct clks_fb_state {
    volatile u8 *address;
    struct clks_framebuffer_info info;
    clks_bool ready;
};

static struct clks_fb_state clks_fb = {
    .address = CLKS_NULL,
    .info = {0, 0, 0, 0},
    .ready = CLKS_FALSE,
};

static void clks_fb_put_pixel(u32 x, u32 y, u32 rgb) {
    volatile u8 *row;
    volatile u32 *pixel;

    if (clks_fb.ready == CLKS_FALSE) {
        return;
    }

    if (x >= clks_fb.info.width || y >= clks_fb.info.height) {
        return;
    }

    if (clks_fb.info.bpp != 32) {
        return;
    }

    row = clks_fb.address + ((usize)y * (usize)clks_fb.info.pitch);
    pixel = (volatile u32 *)(row + ((usize)x * 4));
    *pixel = rgb;
}

void clks_fb_init(const struct limine_framebuffer *fb) {
    if (fb == CLKS_NULL) {
        clks_fb.ready = CLKS_FALSE;
        return;
    }

    clks_fb.address = (volatile u8 *)fb->address;
    clks_fb.info.width = (u32)fb->width;
    clks_fb.info.height = (u32)fb->height;
    clks_fb.info.pitch = (u32)fb->pitch;
    clks_fb.info.bpp = fb->bpp;
    clks_fb.ready = CLKS_TRUE;
}

clks_bool clks_fb_ready(void) {
    return clks_fb.ready;
}

struct clks_framebuffer_info clks_fb_info(void) {
    return clks_fb.info;
}

void clks_fb_clear(u32 rgb) {
    u32 x;
    u32 y;

    if (clks_fb.ready == CLKS_FALSE) {
        return;
    }

    for (y = 0; y < clks_fb.info.height; y++) {
        for (x = 0; x < clks_fb.info.width; x++) {
            clks_fb_put_pixel(x, y, rgb);
        }
    }
}

void clks_fb_draw_char(u32 x, u32 y, char ch, u32 fg_rgb, u32 bg_rgb) {
    const u8 *glyph;
    u32 row;
    u32 col;

    if (clks_fb.ready == CLKS_FALSE) {
        return;
    }

    glyph = clks_font8x8_get(ch);

    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            u8 mask = (u8)(1U << (7U - col));
            u32 color = (glyph[row] & mask) != 0 ? fg_rgb : bg_rgb;
            clks_fb_put_pixel(x + col, y + row, color);
        }
    }
}