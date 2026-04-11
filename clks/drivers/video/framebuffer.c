#include <clks/framebuffer.h>
#include <clks/string.h>
#include <clks/types.h>

#include "psf_font.h"

struct clks_fb_state {
    volatile u8 *address;
    struct clks_framebuffer_info info;
    const struct clks_psf_font *font;
    struct clks_psf_font external_font;
    clks_bool external_font_active;
    u32 glyph_width;
    u32 glyph_height;
    clks_bool ready;
};

static struct clks_fb_state clks_fb = {
    .address = CLKS_NULL,
    .info = {0, 0, 0, 0},
    .font = CLKS_NULL,
    .external_font = {0, 0, 0, 0, 0, CLKS_NULL},
    .external_font_active = CLKS_FALSE,
    .glyph_width = 8U,
    .glyph_height = 8U,
    .ready = CLKS_FALSE,
};

static void clks_fb_apply_font(const struct clks_psf_font *font) {
    clks_fb.font = font;
    clks_fb.glyph_width = 8U;
    clks_fb.glyph_height = 8U;

    if (font != CLKS_NULL) {
        if (font->width != 0U) {
            clks_fb.glyph_width = font->width;
        }

        if (font->height != 0U) {
            clks_fb.glyph_height = font->height;
        }
    }
}

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
    pixel = (volatile u32 *)(row + ((usize)x * 4U));
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

    clks_fb.external_font_active = CLKS_FALSE;
    clks_fb_apply_font(clks_psf_default_font());

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

    for (y = 0U; y < clks_fb.info.height; y++) {
        for (x = 0U; x < clks_fb.info.width; x++) {
            clks_fb_put_pixel(x, y, rgb);
        }
    }
}

void clks_fb_scroll_up(u32 pixel_rows, u32 fill_rgb) {
    usize row_bytes;
    usize move_bytes;
    u32 y;
    u32 x;

    if (clks_fb.ready == CLKS_FALSE) {
        return;
    }

    if (clks_fb.info.bpp != 32) {
        return;
    }

    if (pixel_rows == 0U) {
        return;
    }

    if (pixel_rows >= clks_fb.info.height) {
        clks_fb_clear(fill_rgb);
        return;
    }

    row_bytes = (usize)clks_fb.info.pitch;
    move_bytes = (usize)(clks_fb.info.height - pixel_rows) * row_bytes;

    clks_memmove(
        (void *)clks_fb.address,
        (const void *)(clks_fb.address + ((usize)pixel_rows * row_bytes)),
        move_bytes
    );

    for (y = clks_fb.info.height - pixel_rows; y < clks_fb.info.height; y++) {
        for (x = 0U; x < clks_fb.info.width; x++) {
            clks_fb_put_pixel(x, y, fill_rgb);
        }
    }
}

void clks_fb_draw_char(u32 x, u32 y, char ch, u32 fg_rgb, u32 bg_rgb) {
    const u8 *glyph;
    u32 row;
    u32 col;
    u32 cols;
    u32 rows;
    u32 row_stride;

    if (clks_fb.ready == CLKS_FALSE || clks_fb.font == CLKS_NULL) {
        return;
    }

    glyph = clks_psf_glyph(clks_fb.font, (u32)(u8)ch);

    cols = clks_fb.glyph_width;
    rows = clks_fb.glyph_height;

    if (cols == 0U) {
        cols = 8U;
    }

    if (rows == 0U) {
        rows = 8U;
    }

    row_stride = clks_fb.font->bytes_per_row;

    if (row_stride == 0U) {
        row_stride = (cols + 7U) / 8U;
    }

    if (row_stride == 0U) {
        return;
    }

    if (((usize)row_stride * (usize)rows) > (usize)clks_fb.font->bytes_per_glyph) {
        return;
    }

    for (row = 0U; row < rows; row++) {
        const u8 *row_bits = glyph + ((usize)row * (usize)row_stride);

        for (col = 0U; col < cols; col++) {
            u8 bits = row_bits[col >> 3U];
            u8 mask = (u8)(0x80U >> (col & 7U));
            u32 color = (bits & mask) != 0U ? fg_rgb : bg_rgb;
            clks_fb_put_pixel(x + col, y + row, color);
        }
    }
}

clks_bool clks_fb_load_psf_font(const void *blob, u64 blob_size) {
    struct clks_psf_font parsed = {0, 0, 0, 0, 0, CLKS_NULL};

    if (clks_psf_parse_font(blob, blob_size, &parsed) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_fb.external_font = parsed;
    clks_fb.external_font_active = CLKS_TRUE;
    clks_fb_apply_font(&clks_fb.external_font);
    return CLKS_TRUE;
}

u32 clks_fb_cell_width(void) {
    return clks_fb.glyph_width == 0U ? 8U : clks_fb.glyph_width;
}

u32 clks_fb_cell_height(void) {
    return clks_fb.glyph_height == 0U ? 8U : clks_fb.glyph_height;
}
