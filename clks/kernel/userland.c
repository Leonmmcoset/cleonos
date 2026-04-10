#include <clks/elf64.h>
#include <clks/fs.h>
#include <clks/log.h>
#include <clks/types.h>
#include <clks/userland.h>

static clks_bool clks_userland_probe_elf(const char *path, const char *tag) {
    const void *image;
    u64 size = 0ULL;
    struct clks_elf64_info info;

    image = clks_fs_read_all(path, &size);

    if (image == CLKS_NULL) {
        clks_log(CLKS_LOG_ERROR, "USER", "ELF FILE MISSING");
        clks_log(CLKS_LOG_ERROR, "USER", path);
        return CLKS_FALSE;
    }

    if (clks_elf64_inspect(image, size, &info) == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "USER", "ELF INSPECT FAILED");
        clks_log(CLKS_LOG_ERROR, "USER", path);
        return CLKS_FALSE;
    }

    clks_log(CLKS_LOG_INFO, "USER", tag);
    clks_log_hex(CLKS_LOG_INFO, "USER", "ELF_SIZE", size);
    clks_log_hex(CLKS_LOG_INFO, "USER", "ENTRY", info.entry);
    return CLKS_TRUE;
}

clks_bool clks_userland_init(void) {
    clks_log(CLKS_LOG_INFO, "USER", "USERLAND FRAMEWORK ONLINE");

    if (clks_userland_probe_elf("/shell/shell.elf", "SHELL ELF READY") == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_userland_probe_elf("/system/elfrunner.elf", "ELFRUNNER ELF READY") == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_userland_probe_elf("/system/memc.elf", "MEMC ELF READY") == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

