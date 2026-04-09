#ifndef CLKS_LIMINE_H
#define CLKS_LIMINE_H

#include <clks/types.h>

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88ULL
#define LIMINE_REQUEST_MAGIC 0x0a82e883a194f07bULL

#define LIMINE_REQUESTS_START_MARKER \
    { 0xf6b8f4b39de7d1aeULL, 0xfab91a6940fcb9cfULL }

#define LIMINE_REQUESTS_END_MARKER \
    { 0xadc0e0531bb10d03ULL, 0x9572709f31764c62ULL }

#define LIMINE_BASE_REVISION(N) \
    { 0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, (N) }

#define LIMINE_FRAMEBUFFER_REQUEST \
    { \
        LIMINE_COMMON_MAGIC, \
        LIMINE_REQUEST_MAGIC, \
        0x9d5827dcd881dd75ULL, \
        0xa3148604f6fab11bULL \
    }

struct limine_framebuffer {
    void *address;
    u64 width;
    u64 height;
    u64 pitch;
    u16 bpp;
    u8 memory_model;
    u8 red_mask_size;
    u8 red_mask_shift;
    u8 green_mask_size;
    u8 green_mask_shift;
    u8 blue_mask_size;
    u8 blue_mask_shift;
    u8 unused[7];
    u64 edid_size;
    void *edid;
};

struct limine_framebuffer_response {
    u64 revision;
    u64 framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    u64 id[4];
    u64 revision;
    struct limine_framebuffer_response *response;
};

#endif