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

#define LIMINE_MEMMAP_REQUEST \
    { \
        LIMINE_COMMON_MAGIC, \
        LIMINE_REQUEST_MAGIC, \
        0x67cf3d9d378a806fULL, \
        0xe304acdfc50c3c62ULL \
    }

#define LIMINE_EXECUTABLE_FILE_REQUEST \
    { \
        LIMINE_COMMON_MAGIC, \
        LIMINE_REQUEST_MAGIC, \
        0xad97e90e83f1ed67ULL, \
        0x31eb5d1c5ff23b69ULL \
    }

#define LIMINE_MODULE_REQUEST \
    { \
        LIMINE_COMMON_MAGIC, \
        LIMINE_REQUEST_MAGIC, \
        0x3e7e279702be32afULL, \
        0xca1c4f3bd1280ceeULL \
    }

#define LIMINE_MEMMAP_USABLE                 0ULL
#define LIMINE_MEMMAP_RESERVED               1ULL
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2ULL
#define LIMINE_MEMMAP_ACPI_NVS               3ULL
#define LIMINE_MEMMAP_BAD_MEMORY             4ULL
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5ULL
#define LIMINE_MEMMAP_EXECUTABLE_AND_MODULES 6ULL
#define LIMINE_MEMMAP_FRAMEBUFFER            7ULL
#define LIMINE_MEMMAP_RESERVED_MAPPED        8ULL

struct limine_uuid {
    u32 a;
    u16 b;
    u16 c;
    u8 d[8];
};

struct limine_file {
    u64 revision;
    void *address;
    u64 size;
    char *path;
    char *string;
    u32 media_type;
    u32 unused;
    u32 tftp_ip;
    u32 tftp_port;
    u32 partition_index;
    u32 mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};

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

struct limine_memmap_entry {
    u64 base;
    u64 length;
    u64 type;
};

struct limine_memmap_response {
    u64 revision;
    u64 entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    u64 id[4];
    u64 revision;
    struct limine_memmap_response *response;
};

struct limine_executable_file_response {
    u64 revision;
    struct limine_file *executable_file;
};

struct limine_executable_file_request {
    u64 id[4];
    u64 revision;
    struct limine_executable_file_response *response;
};

struct limine_module_response {
    u64 revision;
    u64 module_count;
    struct limine_file **modules;
};

struct limine_module_request {
    u64 id[4];
    u64 revision;
    struct limine_module_response *response;
};

#endif
