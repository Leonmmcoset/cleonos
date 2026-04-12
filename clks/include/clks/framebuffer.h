#ifndef CLKS_FRAMEBUFFER_H
#define CLKS_FRAMEBUFFER_H

#include <clks/limine.h>
#include <clks/types.h>

struct clks_framebuffer_info {
    u32 width;
    u32 height;
    u32 pitch;
    u16 bpp;
};

void clks_fb_init(const struct limine_framebuffer *fb);
clks_bool clks_fb_ready(void);
struct clks_framebuffer_info clks_fb_info(void);
void clks_fb_clear(u32 rgb);
void clks_fb_scroll_up(u32 pixel_rows, u32 fill_rgb);
void clks_fb_draw_pixel(u32 x, u32 y, u32 rgb);
clks_bool clks_fb_read_pixel(u32 x, u32 y, u32 *out_rgb);
void clks_fb_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 rgb);
void clks_fb_draw_char(u32 x, u32 y, char ch, u32 fg_rgb, u32 bg_rgb);
clks_bool clks_fb_load_psf_font(const void *blob, u64 blob_size);
u32 clks_fb_cell_width(void);
u32 clks_fb_cell_height(void);

#endif
