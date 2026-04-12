#include <clks/elf64.h>
#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/log.h>
#include <clks/string.h>
#include <clks/types.h>

typedef u64 (*clks_exec_entry_fn)(void);

#define CLKS_EXEC_STATUS_UNSUPPORTED 0xFFFFFFFFFFFFFFFEULL

static u64 clks_exec_requests = 0ULL;
static u64 clks_exec_success = 0ULL;

static clks_bool clks_exec_is_sync_unsupported(const char *path) {
    if (path == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_strcmp(path, "/shell/shell.elf") == 0) {
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

void clks_exec_init(void) {
    clks_exec_requests = 0ULL;
    clks_exec_success = 0ULL;
    clks_log(CLKS_LOG_INFO, "EXEC", "PATH EXEC FRAMEWORK ONLINE");
}

clks_bool clks_exec_run_path(const char *path, u64 *out_status) {
    const void *image;
    u64 image_size = 0ULL;
    struct clks_elf64_info info;
    struct clks_elf64_loaded_image loaded;
    void *entry_ptr;
    u64 run_ret;

    clks_exec_requests++;

    if (out_status != CLKS_NULL) {
        *out_status = (u64)-1;
    }

    if (path == CLKS_NULL || path[0] != '/') {
        clks_log(CLKS_LOG_WARN, "EXEC", "INVALID EXEC PATH");
        return CLKS_FALSE;
    }

    if (clks_exec_is_sync_unsupported(path) == CLKS_TRUE) {
        clks_log(CLKS_LOG_WARN, "EXEC", "SYNC EXEC UNSUPPORTED FOR INTERACTIVE ELF");
        clks_log(CLKS_LOG_WARN, "EXEC", path);

        if (out_status != CLKS_NULL) {
            *out_status = CLKS_EXEC_STATUS_UNSUPPORTED;
        }

        return CLKS_FALSE;
    }

    image = clks_fs_read_all(path, &image_size);

    if (image == CLKS_NULL || image_size == 0ULL) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC FILE MISSING");
        clks_log(CLKS_LOG_WARN, "EXEC", path);
        return CLKS_FALSE;
    }

    if (clks_elf64_inspect(image, image_size, &info) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC ELF INVALID");
        clks_log(CLKS_LOG_WARN, "EXEC", path);
        return CLKS_FALSE;
    }

    if (clks_elf64_load(image, image_size, &loaded) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC ELF LOAD FAILED");
        clks_log(CLKS_LOG_WARN, "EXEC", path);
        return CLKS_FALSE;
    }

    entry_ptr = clks_elf64_entry_pointer(&loaded, info.entry);
    if (entry_ptr == CLKS_NULL) {
        clks_log(CLKS_LOG_WARN, "EXEC", "ENTRY POINTER RESOLVE FAILED");
        clks_log(CLKS_LOG_WARN, "EXEC", path);
        clks_elf64_unload(&loaded);
        return CLKS_FALSE;
    }

    clks_log(CLKS_LOG_INFO, "EXEC", "EXEC RUN START");
    clks_log(CLKS_LOG_INFO, "EXEC", path);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "ENTRY", info.entry);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "PHNUM", (u64)info.phnum);

    run_ret = ((clks_exec_entry_fn)entry_ptr)();

    clks_log(CLKS_LOG_INFO, "EXEC", "RUN RETURNED");
    clks_log(CLKS_LOG_INFO, "EXEC", path);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "RET", run_ret);

    clks_exec_success++;

    if (out_status != CLKS_NULL) {
        *out_status = run_ret;
    }

    clks_elf64_unload(&loaded);
    return CLKS_TRUE;
}

u64 clks_exec_request_count(void) {
    return clks_exec_requests;
}

u64 clks_exec_success_count(void) {
    return clks_exec_success;
}
