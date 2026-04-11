#ifndef CLKS_PSF_FONT_H
#define CLKS_PSF_FONT_H

#include <clks/types.h>

struct clks_psf_font {
    u32 width;
    u32 height;
    u32 glyph_count;
    u32 bytes_per_glyph;
    const u8 *glyphs;
};

const struct clks_psf_font *clks_psf_default_font(void);
const u8 *clks_psf_glyph(const struct clks_psf_font *font, u32 codepoint);
clks_bool clks_psf_parse_font(const void *blob, u64 blob_size, struct clks_psf_font *out_font);

#endif