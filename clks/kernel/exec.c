#include <clks/cpu.h>
#include <clks/elf64.h>
#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/interrupts.h>
#include <clks/log.h>
#include <clks/string.h>
#include <clks/types.h>
#include <clks/tty.h>

typedef u64 (*clks_exec_entry_fn)(void);

#define CLKS_EXEC_RUN_STACK_BYTES (64ULL * 1024ULL)
#define CLKS_EXEC_MAX_PROCS       64U
#define CLKS_EXEC_MAX_DEPTH       16U
#define CLKS_EXEC_PATH_MAX        192U

enum clks_exec_proc_state {
    CLKS_EXEC_PROC_UNUSED = 0,
    CLKS_EXEC_PROC_PENDING = 1,
    CLKS_EXEC_PROC_RUNNING = 2,
    CLKS_EXEC_PROC_EXITED = 3,
};

struct clks_exec_proc_record {
    clks_bool used;
    enum clks_exec_proc_state state;
    u64 pid;
    u64 ppid;
    u64 started_tick;
    u64 exited_tick;
    u64 exit_status;
    u32 tty_index;
    char path[CLKS_EXEC_PATH_MAX];
};

#if defined(CLKS_ARCH_X86_64)
extern u64 clks_exec_call_on_stack_x86_64(void *entry_ptr, void *stack_top);
#endif

static u64 clks_exec_requests = 0ULL;
static u64 clks_exec_success = 0ULL;
static u32 clks_exec_running_depth = 0U;
static clks_bool clks_exec_pending_dispatch_active = CLKS_FALSE;

static struct clks_exec_proc_record clks_exec_proc_table[CLKS_EXEC_MAX_PROCS];
static u64 clks_exec_next_pid = 1ULL;
static u64 clks_exec_pid_stack[CLKS_EXEC_MAX_DEPTH];
static clks_bool clks_exec_exit_requested_stack[CLKS_EXEC_MAX_DEPTH];
static u64 clks_exec_exit_status_stack[CLKS_EXEC_MAX_DEPTH];
static u32 clks_exec_pid_stack_depth = 0U;

static clks_bool clks_exec_has_prefix(const char *text, const char *prefix) {
    usize i = 0U;

    if (text == CLKS_NULL || prefix == CLKS_NULL) {
        return CLKS_FALSE;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return CLKS_FALSE;
        }

        i++;
    }

    return CLKS_TRUE;
}

static clks_bool clks_exec_has_suffix(const char *text, const char *suffix) {
    usize text_len;
    usize suffix_len;
    usize i;

    if (text == CLKS_NULL || suffix == CLKS_NULL) {
        return CLKS_FALSE;
    }

    text_len = clks_strlen(text);
    suffix_len = clks_strlen(suffix);

    if (suffix_len > text_len) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < suffix_len; i++) {
        if (text[text_len - suffix_len + i] != suffix[i]) {
            return CLKS_FALSE;
        }
    }

    return CLKS_TRUE;
}

static clks_bool clks_exec_path_is_user_program(const char *path) {
    if (path == CLKS_NULL || path[0] != '/') {
        return CLKS_FALSE;
    }

    if (clks_exec_has_prefix(path, "/system/") == CLKS_TRUE) {
        return CLKS_FALSE;
    }

    if (clks_exec_has_prefix(path, "/driver/") == CLKS_TRUE) {
        return CLKS_FALSE;
    }

    if (clks_exec_has_prefix(path, "/shell/") == CLKS_TRUE) {
        return CLKS_TRUE;
    }

    return clks_exec_has_suffix(path, ".elf");
}

static void clks_exec_copy_path(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || src == CLKS_NULL || dst_size == 0U) {
        return;
    }

    while (src[i] != '\0' && i + 1U < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static i32 clks_exec_proc_find_slot_by_pid(u64 pid) {
    u32 i;

    for (i = 0U; i < CLKS_EXEC_MAX_PROCS; i++) {
        if (clks_exec_proc_table[i].used == CLKS_TRUE && clks_exec_proc_table[i].pid == pid) {
            return (i32)i;
        }
    }

    return -1;
}

static i32 clks_exec_proc_alloc_slot(void) {
    u32 i;

    for (i = 0U; i < CLKS_EXEC_MAX_PROCS; i++) {
        if (clks_exec_proc_table[i].used == CLKS_FALSE) {
            return (i32)i;
        }
    }

    for (i = 0U; i < CLKS_EXEC_MAX_PROCS; i++) {
        if (clks_exec_proc_table[i].state == CLKS_EXEC_PROC_EXITED) {
            return (i32)i;
        }
    }

    return -1;
}

static i32 clks_exec_current_depth_index(void) {
    if (clks_exec_pid_stack_depth == 0U) {
        return -1;
    }

    return (i32)(clks_exec_pid_stack_depth - 1U);
}

static u64 clks_exec_alloc_pid(void) {
    u64 pid = clks_exec_next_pid;

    clks_exec_next_pid++;

    if (clks_exec_next_pid == 0ULL) {
        clks_exec_next_pid = 1ULL;
    }

    if (pid == 0ULL) {
        pid = clks_exec_next_pid;
        clks_exec_next_pid++;

        if (clks_exec_next_pid == 0ULL) {
            clks_exec_next_pid = 1ULL;
        }
    }

    return pid;
}

static struct clks_exec_proc_record *clks_exec_prepare_proc_record(i32 slot,
                                                                    u64 pid,
                                                                    const char *path,
                                                                    enum clks_exec_proc_state state) {
    struct clks_exec_proc_record *proc;

    if (slot < 0 || path == CLKS_NULL) {
        return CLKS_NULL;
    }

    proc = &clks_exec_proc_table[(u32)slot];
    clks_memset(proc, 0, sizeof(*proc));

    proc->used = CLKS_TRUE;
    proc->state = state;
    proc->pid = pid;
    proc->ppid = clks_exec_current_pid();
    proc->started_tick = 0ULL;
    proc->exited_tick = 0ULL;
    proc->exit_status = (u64)-1;
    proc->tty_index = clks_tty_active();
    clks_exec_copy_path(proc->path, sizeof(proc->path), path);
    return proc;
}

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

static clks_bool clks_exec_run_proc_slot(i32 slot, u64 *out_status) {
    struct clks_exec_proc_record *proc;
    const void *image;
    u64 image_size = 0ULL;
    struct clks_elf64_info info;
    struct clks_elf64_loaded_image loaded;
    clks_bool loaded_active = CLKS_FALSE;
    clks_bool depth_pushed = CLKS_FALSE;
    clks_bool depth_counted = CLKS_FALSE;
    void *entry_ptr;
    u64 run_ret = (u64)-1;
    i32 depth_index;

    clks_memset(&loaded, 0, sizeof(loaded));

    if (out_status != CLKS_NULL) {
        *out_status = (u64)-1;
    }

    if (slot < 0 || (u32)slot >= CLKS_EXEC_MAX_PROCS) {
        return CLKS_FALSE;
    }

    proc = &clks_exec_proc_table[(u32)slot];

    if (proc->used == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (proc->path[0] != '/') {
        proc->state = CLKS_EXEC_PROC_EXITED;
        proc->exit_status = (u64)-1;
        proc->exited_tick = clks_interrupts_timer_ticks();
        return CLKS_FALSE;
    }

    if (clks_exec_pid_stack_depth >= CLKS_EXEC_MAX_DEPTH) {
        clks_log(CLKS_LOG_WARN, "EXEC", "PROCESS STACK DEPTH EXCEEDED");
        proc->state = CLKS_EXEC_PROC_EXITED;
        proc->exit_status = (u64)-1;
        proc->exited_tick = clks_interrupts_timer_ticks();
        return CLKS_FALSE;
    }

    proc->state = CLKS_EXEC_PROC_RUNNING;
    proc->started_tick = clks_interrupts_timer_ticks();

    depth_index = (i32)clks_exec_pid_stack_depth;
    clks_exec_pid_stack[(u32)depth_index] = proc->pid;
    clks_exec_exit_requested_stack[(u32)depth_index] = CLKS_FALSE;
    clks_exec_exit_status_stack[(u32)depth_index] = 0ULL;
    clks_exec_pid_stack_depth++;
    depth_pushed = CLKS_TRUE;

    image = clks_fs_read_all(proc->path, &image_size);

    if (image == CLKS_NULL || image_size == 0ULL) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC FILE MISSING");
        clks_log(CLKS_LOG_WARN, "EXEC", proc->path);
        goto fail;
    }

    if (clks_elf64_inspect(image, image_size, &info) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC ELF INVALID");
        clks_log(CLKS_LOG_WARN, "EXEC", proc->path);
        goto fail;
    }

    if (clks_elf64_load(image, image_size, &loaded) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC ELF LOAD FAILED");
        clks_log(CLKS_LOG_WARN, "EXEC", proc->path);
        goto fail;
    }

    loaded_active = CLKS_TRUE;

    entry_ptr = clks_elf64_entry_pointer(&loaded, info.entry);
    if (entry_ptr == CLKS_NULL) {
        clks_log(CLKS_LOG_WARN, "EXEC", "ENTRY POINTER RESOLVE FAILED");
        clks_log(CLKS_LOG_WARN, "EXEC", proc->path);
        goto fail;
    }

    clks_log(CLKS_LOG_INFO, "EXEC", "EXEC RUN START");
    clks_log(CLKS_LOG_INFO, "EXEC", proc->path);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "ENTRY", info.entry);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "PHNUM", (u64)info.phnum);

    clks_exec_running_depth++;
    depth_counted = CLKS_TRUE;

    if (clks_exec_invoke_entry(entry_ptr, &run_ret) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "EXEC", "EXEC RUN INVOKE FAILED");
        clks_log(CLKS_LOG_WARN, "EXEC", proc->path);
        goto fail;
    }

    if (depth_counted == CLKS_TRUE && clks_exec_running_depth > 0U) {
        clks_exec_running_depth--;
        depth_counted = CLKS_FALSE;
    }

    if (clks_exec_exit_requested_stack[(u32)depth_index] == CLKS_TRUE) {
        run_ret = clks_exec_exit_status_stack[(u32)depth_index];
    }

    clks_log(CLKS_LOG_INFO, "EXEC", "RUN RETURNED");
    clks_log(CLKS_LOG_INFO, "EXEC", proc->path);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "RET", run_ret);

    clks_exec_success++;

    proc->state = CLKS_EXEC_PROC_EXITED;
    proc->exit_status = run_ret;
    proc->exited_tick = clks_interrupts_timer_ticks();

    if (depth_pushed == CLKS_TRUE && clks_exec_pid_stack_depth > 0U) {
        clks_exec_pid_stack_depth--;
        depth_pushed = CLKS_FALSE;
    }

    if (loaded_active == CLKS_TRUE) {
        clks_elf64_unload(&loaded);
    }

    if (out_status != CLKS_NULL) {
        *out_status = run_ret;
    }

    return CLKS_TRUE;

fail:
    if (depth_counted == CLKS_TRUE && clks_exec_running_depth > 0U) {
        clks_exec_running_depth--;
    }

    proc->state = CLKS_EXEC_PROC_EXITED;
    proc->exit_status = (u64)-1;
    proc->exited_tick = clks_interrupts_timer_ticks();

    if (depth_pushed == CLKS_TRUE && clks_exec_pid_stack_depth > 0U) {
        clks_exec_pid_stack_depth--;
    }

    if (loaded_active == CLKS_TRUE) {
        clks_elf64_unload(&loaded);
    }

    if (out_status != CLKS_NULL) {
        *out_status = (u64)-1;
    }

    return CLKS_FALSE;
}

static clks_bool clks_exec_dispatch_pending_once(void) {
    u32 i;

    if (clks_exec_pending_dispatch_active == CLKS_TRUE) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < CLKS_EXEC_MAX_PROCS; i++) {
        if (clks_exec_proc_table[i].used == CLKS_TRUE &&
            clks_exec_proc_table[i].state == CLKS_EXEC_PROC_PENDING) {
            u64 ignored_status = (u64)-1;

            clks_exec_pending_dispatch_active = CLKS_TRUE;
            (void)clks_exec_run_proc_slot((i32)i, &ignored_status);
            clks_exec_pending_dispatch_active = CLKS_FALSE;
            return CLKS_TRUE;
        }
    }

    return CLKS_FALSE;
}

static clks_bool clks_exec_run_path_internal(const char *path, u64 *out_status, u64 *out_pid) {
    i32 slot;
    u64 pid;
    struct clks_exec_proc_record *proc;
    u64 status = (u64)-1;

    if (out_status != CLKS_NULL) {
        *out_status = (u64)-1;
    }

    if (out_pid != CLKS_NULL) {
        *out_pid = (u64)-1;
    }

    clks_exec_requests++;

    if (path == CLKS_NULL || path[0] != '/') {
        clks_log(CLKS_LOG_WARN, "EXEC", "INVALID EXEC PATH");
        return CLKS_FALSE;
    }

    slot = clks_exec_proc_alloc_slot();

    if (slot < 0) {
        clks_log(CLKS_LOG_WARN, "EXEC", "PROCESS TABLE FULL");
        return CLKS_FALSE;
    }

    pid = clks_exec_alloc_pid();
    proc = clks_exec_prepare_proc_record(slot, pid, path, CLKS_EXEC_PROC_RUNNING);

    if (proc == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (out_pid != CLKS_NULL) {
        *out_pid = pid;
    }

    if (clks_exec_run_proc_slot(slot, &status) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (out_status != CLKS_NULL) {
        *out_status = status;
    }

    return CLKS_TRUE;
}

void clks_exec_init(void) {
    clks_exec_requests = 0ULL;
    clks_exec_success = 0ULL;
    clks_exec_running_depth = 0U;
    clks_exec_pending_dispatch_active = CLKS_FALSE;
    clks_exec_next_pid = 1ULL;
    clks_exec_pid_stack_depth = 0U;
    clks_memset(clks_exec_pid_stack, 0, sizeof(clks_exec_pid_stack));
    clks_memset(clks_exec_exit_requested_stack, 0, sizeof(clks_exec_exit_requested_stack));
    clks_memset(clks_exec_exit_status_stack, 0, sizeof(clks_exec_exit_status_stack));
    clks_memset(clks_exec_proc_table, 0, sizeof(clks_exec_proc_table));
    clks_log(CLKS_LOG_INFO, "EXEC", "PATH EXEC FRAMEWORK ONLINE");
}

clks_bool clks_exec_run_path(const char *path, u64 *out_status) {
    return clks_exec_run_path_internal(path, out_status, CLKS_NULL);
}

clks_bool clks_exec_spawn_path(const char *path, u64 *out_pid) {
    i32 slot;
    u64 pid;
    struct clks_exec_proc_record *proc;

    if (out_pid != CLKS_NULL) {
        *out_pid = (u64)-1;
    }

    clks_exec_requests++;

    if (path == CLKS_NULL || path[0] != '/') {
        clks_log(CLKS_LOG_WARN, "EXEC", "INVALID SPAWN PATH");
        return CLKS_FALSE;
    }

    slot = clks_exec_proc_alloc_slot();

    if (slot < 0) {
        clks_log(CLKS_LOG_WARN, "EXEC", "PROCESS TABLE FULL");
        return CLKS_FALSE;
    }

    pid = clks_exec_alloc_pid();
    proc = clks_exec_prepare_proc_record(slot, pid, path, CLKS_EXEC_PROC_PENDING);

    if (proc == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (out_pid != CLKS_NULL) {
        *out_pid = pid;
    }

    clks_log(CLKS_LOG_INFO, "EXEC", "SPAWN QUEUED");
    clks_log(CLKS_LOG_INFO, "EXEC", path);
    clks_log_hex(CLKS_LOG_INFO, "EXEC", "PID", pid);

    return CLKS_TRUE;
}

u64 clks_exec_wait_pid(u64 pid, u64 *out_status) {
    i32 slot;
    struct clks_exec_proc_record *proc;

    slot = clks_exec_proc_find_slot_by_pid(pid);

    if (slot < 0) {
        return (u64)-1;
    }

    proc = &clks_exec_proc_table[(u32)slot];

    if (proc->used == CLKS_FALSE) {
        return (u64)-1;
    }

    if (proc->state == CLKS_EXEC_PROC_PENDING && clks_exec_pending_dispatch_active == CLKS_FALSE) {
        u64 ignored_status = (u64)-1;

        clks_exec_pending_dispatch_active = CLKS_TRUE;
        (void)clks_exec_run_proc_slot(slot, &ignored_status);
        clks_exec_pending_dispatch_active = CLKS_FALSE;
    }

    if (proc->state == CLKS_EXEC_PROC_PENDING || proc->state == CLKS_EXEC_PROC_RUNNING) {
        return 0ULL;
    }

    if (proc->state != CLKS_EXEC_PROC_EXITED) {
        return (u64)-1;
    }

    if (out_status != CLKS_NULL) {
        *out_status = proc->exit_status;
    }

    return 1ULL;
}

clks_bool clks_exec_request_exit(u64 status) {
    i32 depth_index = clks_exec_current_depth_index();

    if (depth_index < 0) {
        return CLKS_FALSE;
    }

    clks_exec_exit_requested_stack[(u32)depth_index] = CLKS_TRUE;
    clks_exec_exit_status_stack[(u32)depth_index] = status;
    return CLKS_TRUE;
}

u64 clks_exec_current_pid(void) {
    i32 depth_index = clks_exec_current_depth_index();

    if (depth_index < 0) {
        return 0ULL;
    }

    return clks_exec_pid_stack[(u32)depth_index];
}

u32 clks_exec_current_tty(void) {
    i32 depth_index = clks_exec_current_depth_index();
    u32 tty_count = clks_tty_count();
    i32 slot;
    const struct clks_exec_proc_record *proc;

    if (tty_count == 0U) {
        return 0U;
    }

    if (depth_index < 0) {
        u32 active = clks_tty_active();
        return (active < tty_count) ? active : 0U;
    }

    slot = clks_exec_proc_find_slot_by_pid(clks_exec_pid_stack[(u32)depth_index]);

    if (slot < 0) {
        return 0U;
    }

    proc = &clks_exec_proc_table[(u32)slot];

    if (proc->used == CLKS_FALSE || proc->tty_index >= tty_count) {
        return 0U;
    }

    return proc->tty_index;
}

u64 clks_exec_sleep_ticks(u64 ticks) {
    u64 start = clks_interrupts_timer_ticks();

    if (ticks == 0ULL) {
        return 0ULL;
    }

    while ((clks_interrupts_timer_ticks() - start) < ticks) {
#if defined(CLKS_ARCH_X86_64)
        __asm__ volatile("sti; hlt; cli" : : : "memory");
#elif defined(CLKS_ARCH_AARCH64)
        clks_cpu_pause();
#endif
        (void)clks_exec_dispatch_pending_once();
    }

    return clks_interrupts_timer_ticks() - start;
}

u64 clks_exec_yield(void) {
#if defined(CLKS_ARCH_X86_64)
    __asm__ volatile("sti; hlt; cli" : : : "memory");
#elif defined(CLKS_ARCH_AARCH64)
    clks_cpu_pause();
#endif

    (void)clks_exec_dispatch_pending_once();
    return clks_interrupts_timer_ticks();
}

void clks_exec_tick(u64 tick) {
    (void)tick;
    (void)clks_exec_dispatch_pending_once();
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

clks_bool clks_exec_current_path_is_user(void) {
    i32 depth_index;
    i32 slot;
    const struct clks_exec_proc_record *proc;

    depth_index = clks_exec_current_depth_index();

    if (depth_index < 0) {
        return CLKS_FALSE;
    }

    slot = clks_exec_proc_find_slot_by_pid(clks_exec_pid_stack[(u32)depth_index]);

    if (slot < 0) {
        return CLKS_FALSE;
    }

    proc = &clks_exec_proc_table[(u32)slot];
    return clks_exec_path_is_user_program(proc->path);
}
