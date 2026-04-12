#include <clks/elf64.h>
#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/log.h>
#include <clks/types.h>

typedef u64 (*clks_exec_entry_fn)(void);

#define CLKS_EXEC_RUN_STACK_BYTES (64ULL * 1024ULL)

#if defined(CLKS_ARCH_X86_64)
extern u64 clks_exec_call_on_stack_x86_64(void *entry_ptr, void *stack_top);
#endif

static u64 clks_exec_requests = 0ULL;
static u64 clks_exec_success = 0ULL;
static u32 clks_exec_running_depth = 0U;

static clks_bool clks_exec_invoke_entry(void *entry_ptr, u64 *out_ret) {
    if (entry_ptr == CLKS_NULL || out_ret == CLKS_NULL) {
        return CLKS_FALSE;
    }

#if defined(CLKS_ARCH_X86_64)
    {
        void *stack_base = clks_kmalloc((usize)CLKS_EXEC_RUN_STACK_BYTES);
        void *stack_top;

        if (stack_base == CLKS_NULL) {
            clks_log(CLKS_LOG_WARN, "EXEC", "RUN STACK ALLOC FAILED");
            return CLKS_FALSE;
        }

        stack_top = (void *)((u8 *)stack_base + (usize)CLKS_EXEC_RUN_STACK_BYTES);
        *out_ret = clks_exec_call_on_stack_x86_64(entry_ptr, stack_top);
        clks_kfree(stack_base);
        return CLKS_TRUE;
    }
#else
    *out_ret = ((clks_exec_entry_fn)entry_ptr)();
    return CLKS_TRUE;
#endif
}

void clks_exec_init(void) {
    clks_exec_requests = 0ULL;
    clks_exec_success = 0ULL;
    clks_exec_running_depth = 0U;
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

    clks_exec_running_depth++;
    if (clks_exec_invoke_entry(entry_ptr, &run_ret) == CLKS_FALSE) {
        if (clks_exec_running_depth > 0U) {
            clks_exec_running_depth--;
        }

        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC RUN INVOKE FAILED");
        clks_log(CLKS_LOG_WARN, "EXEC", path);
        clks_elf64_unload(&loaded);
        return CLKS_FALSE;
    }
    if (clks_exec_running_depth > 0U) {
        clks_exec_running_depth--;
    }

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

clks_bool clks_exec_is_running(void) {
    return (clks_exec_running_depth > 0U) ? CLKS_TRUE : CLKS_FALSE;
}

