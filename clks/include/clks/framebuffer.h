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
void clks_fb_draw_char(u32 x, u32 y, char ch, u32 fg_rgb, u32 bg_rgb);

#endif