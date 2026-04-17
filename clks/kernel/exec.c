#include <clks/cpu.h>
#include <clks/elf64.h>
#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/interrupts.h>
#include <clks/log.h>
#include <clks/serial.h>
#include <clks/string.h>
#include <clks/types.h>
#include <clks/tty.h>

typedef u64 (*clks_exec_entry_fn)(void);

#define CLKS_EXEC_RUN_STACK_BYTES (64ULL * 1024ULL)
#define CLKS_EXEC_MAX_PROCS       64U
#define CLKS_EXEC_MAX_DEPTH       16U
#define CLKS_EXEC_PATH_MAX        192U
#define CLKS_EXEC_ARG_LINE_MAX    256U
#define CLKS_EXEC_ENV_LINE_MAX    512U
#define CLKS_EXEC_MAX_ARGS         24U
#define CLKS_EXEC_MAX_ENVS         24U
#define CLKS_EXEC_ITEM_MAX        128U
#define CLKS_EXEC_STATUS_SIGNAL_FLAG (1ULL << 63)
#define CLKS_EXEC_DEFAULT_KILL_SIGNAL CLKS_EXEC_SIGNAL_TERM

enum clks_exec_proc_state {
    CLKS_EXEC_PROC_UNUSED = 0,
    CLKS_EXEC_PROC_PENDING = 1,
    CLKS_EXEC_PROC_RUNNING = 2,
    CLKS_EXEC_PROC_EXITED = 3,
    CLKS_EXEC_PROC_STOPPED = 4,
};

struct clks_exec_proc_record {
    clks_bool used;
    enum clks_exec_proc_state state;
    u64 pid;
    u64 ppid;
    u64 started_tick;
    u64 exited_tick;
    u64 run_started_tick;
    u64 runtime_ticks_accum;
    u64 exit_status;
    u32 tty_index;
    u64 image_mem_bytes;
    char path[CLKS_EXEC_PATH_MAX];
    char argv_line[CLKS_EXEC_ARG_LINE_MAX];
    char env_line[CLKS_EXEC_ENV_LINE_MAX];
    u32 argc;
    u32 envc;
    char argv_items[CLKS_EXEC_MAX_ARGS][CLKS_EXEC_ITEM_MAX];
    char env_items[CLKS_EXEC_MAX_ENVS][CLKS_EXEC_ITEM_MAX];
    u64 last_signal;
    u64 last_fault_vector;
    u64 last_fault_error;
    u64 last_fault_rip;
};

#if defined(CLKS_ARCH_X86_64)
extern u64 clks_exec_call_on_stack_x86_64(void *entry_ptr, void *stack_top);
extern void clks_exec_abort_to_caller_x86_64(void);
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
static u64 clks_exec_unwind_slot_stack[CLKS_EXEC_MAX_DEPTH];
static clks_bool clks_exec_unwind_slot_valid_stack[CLKS_EXEC_MAX_DEPTH];
static u32 clks_exec_pid_stack_depth = 0U;

static void clks_exec_serial_write_hex64(u64 value) {
    int nibble;

    clks_serial_write("0X");

    for (nibble = 15; nibble >= 0; nibble--) {
        u8 current = (u8)((value >> (nibble * 4)) & 0x0FULL);
        char out = (current < 10U) ? (char)('0' + current) : (char)('A' + (current - 10U));
        clks_serial_write_char(out);
    }
}

static void clks_exec_log_info_serial(const char *message) {
    clks_serial_write("[INFO][EXEC] ");

    if (message != CLKS_NULL) {
        clks_serial_write(message);
    }

    clks_serial_write("\n");
}

static void clks_exec_log_hex_serial(const char *label, u64 value) {
    clks_serial_write("[INFO][EXEC] ");

    if (label != CLKS_NULL) {
        clks_serial_write(label);
    }

    clks_serial_write(": ");
    clks_exec_serial_write_hex64(value);
    clks_serial_write("\n");
}
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

static void clks_exec_copy_line(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || dst_size == 0U) {
        return;
    }

    if (src == CLKS_NULL) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1U < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void clks_exec_clear_items(char items[][CLKS_EXEC_ITEM_MAX], u32 max_count) {
    u32 i;

    for (i = 0U; i < max_count; i++) {
        items[i][0] = '\0';
    }
}

static u32 clks_exec_parse_whitespace_items(const char *line,
                                            char items[][CLKS_EXEC_ITEM_MAX],
                                            u32 max_count) {
    u32 count = 0U;
    usize i = 0U;

    if (line == CLKS_NULL || items == CLKS_NULL || max_count == 0U) {
        return 0U;
    }

    while (line[i] != '\0') {
        usize p = 0U;

        while (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n') {
            i++;
        }

        if (line[i] == '\0') {
            break;
        }

        if (count >= max_count) {
            break;
        }

        while (line[i] != '\0' &&
               line[i] != ' ' &&
               line[i] != '\t' &&
               line[i] != '\r' &&
               line[i] != '\n') {
            if (p + 1U < CLKS_EXEC_ITEM_MAX) {
                items[count][p++] = line[i];
            }

            i++;
        }

        items[count][p] = '\0';
        count++;
    }

    return count;
}

static u32 clks_exec_parse_env_items(const char *line,
                                     char items[][CLKS_EXEC_ITEM_MAX],
                                     u32 max_count) {
    u32 count = 0U;
    usize i = 0U;

    if (line == CLKS_NULL || items == CLKS_NULL || max_count == 0U) {
        return 0U;
    }

    while (line[i] != '\0') {
        usize p = 0U;
        usize end_trim = 0U;

        while (line[i] == ' ' || line[i] == '\t' || line[i] == ';' || line[i] == '\r' || line[i] == '\n') {
            i++;
        }

        if (line[i] == '\0') {
            break;
        }

        if (count >= max_count) {
            break;
        }

        while (line[i] != '\0' && line[i] != ';' && line[i] != '\r' && line[i] != '\n') {
            if (p + 1U < CLKS_EXEC_ITEM_MAX) {
                items[count][p++] = line[i];
            }
            i++;
        }

        while (p > 0U && (items[count][p - 1U] == ' ' || items[count][p - 1U] == '\t')) {
            p--;
        }

        end_trim = p;
        items[count][end_trim] = '\0';

        if (end_trim > 0U) {
            count++;
        }
    }

    return count;
}

static u64 clks_exec_signal_from_vector(u64 vector) {
    switch (vector) {
        case 0ULL:
        case 16ULL:
        case 19ULL:
            return 8ULL;
        case 6ULL:
            return 4ULL;
        case 3ULL:
            return 5ULL;
        case 14ULL:
        case 13ULL:
        case 12ULL:
        case 11ULL:
        case 10ULL:
        case 17ULL:
            return 11ULL;
        default:
            return 6ULL;
    }
}

static u64 clks_exec_encode_signal_status(u64 signal, u64 vector, u64 error_code) {
    return CLKS_EXEC_STATUS_SIGNAL_FLAG |
           (signal & 0xFFULL) |
           ((vector & 0xFFULL) << 8) |
           ((error_code & 0xFFFFULL) << 16);
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
                                                                    const char *argv_line,
                                                                    const char *env_line,
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
    proc->run_started_tick = 0ULL;
    proc->runtime_ticks_accum = 0ULL;
    proc->exit_status = (u64)-1;
    proc->tty_index = clks_tty_active();
    proc->image_mem_bytes = 0ULL;
    clks_exec_copy_path(proc->path, sizeof(proc->path), path);
    clks_exec_copy_line(proc->argv_line, sizeof(proc->argv_line), argv_line);
    clks_exec_copy_line(proc->env_line, sizeof(proc->env_line), env_line);
    clks_exec_clear_items(proc->argv_items, CLKS_EXEC_MAX_ARGS);
    clks_exec_clear_items(proc->env_items, CLKS_EXEC_MAX_ENVS);
    proc->argc = 0U;
    proc->envc = 0U;
    proc->last_signal = 0ULL;
    proc->last_fault_vector = 0ULL;
    proc->last_fault_error = 0ULL;
    proc->last_fault_rip = 0ULL;

    if (proc->argc < CLKS_EXEC_MAX_ARGS) {
        clks_exec_copy_path(proc->argv_items[proc->argc], CLKS_EXEC_ITEM_MAX, path);
        proc->argc++;
    }

    if (proc->argv_line[0] != '\0' && proc->argc < CLKS_EXEC_MAX_ARGS) {
        proc->argc += clks_exec_parse_whitespace_items(proc->argv_line,
                                                       &proc->argv_items[proc->argc],
                                                       (u32)(CLKS_EXEC_MAX_ARGS - proc->argc));
    }

    if (proc->env_line[0] != '\0') {
        proc->envc = clks_exec_parse_env_items(proc->env_line, proc->env_items, CLKS_EXEC_MAX_ENVS);
    }

    return proc;
}

static void clks_exec_proc_mark_running(struct clks_exec_proc_record *proc, u64 now_tick) {
    if (proc == CLKS_NULL) {
        return;
    }

    if (proc->started_tick == 0ULL) {
        proc->started_tick = now_tick;
    }

    proc->run_started_tick = now_tick;
    proc->state = CLKS_EXEC_PROC_RUNNING;
}

static void clks_exec_proc_pause_runtime(struct clks_exec_proc_record *proc, u64 now_tick) {
    if (proc == CLKS_NULL) {
        return;
    }

    if (proc->run_started_tick != 0ULL && now_tick > proc->run_started_tick) {
        proc->runtime_ticks_accum += (now_tick - proc->run_started_tick);
    }

    proc->run_started_tick = 0ULL;
}

static void clks_exec_proc_mark_exited(struct clks_exec_proc_record *proc, u64 now_tick, u64 status) {
    if (proc == CLKS_NULL) {
        return;
    }

    clks_exec_proc_pause_runtime(proc, now_tick);
    proc->state = CLKS_EXEC_PROC_EXITED;
    proc->exit_status = status;
    proc->exited_tick = now_tick;
}

static clks_bool clks_exec_invoke_entry(void *entry_ptr, u32 depth_index, u64 *out_ret) {
    if (entry_ptr == CLKS_NULL || out_ret == CLKS_NULL) {
        return CLKS_FALSE;
    }

#if defined(CLKS_ARCH_X86_64)
    {
        void *stack_base = clks_kmalloc((usize)CLKS_EXEC_RUN_STACK_BYTES);
        void *stack_top;
        u64 unwind_slot;

        if (stack_base == CLKS_NULL) {
            clks_log(CLKS_LOG_WARN, "EXEC", "RUN STACK ALLOC FAILED");
            return CLKS_FALSE;
        }

        stack_top = (void *)((u8 *)stack_base + (usize)CLKS_EXEC_RUN_STACK_BYTES);
        unwind_slot = (((u64)stack_top) & ~0xFULL) - 8ULL;
        clks_exec_unwind_slot_stack[depth_index] = unwind_slot;
        clks_exec_unwind_slot_valid_stack[depth_index] = CLKS_TRUE;
        *out_ret = clks_exec_call_on_stack_x86_64(entry_ptr, stack_top);
        clks_exec_unwind_slot_valid_stack[depth_index] = CLKS_FALSE;
        clks_exec_unwind_slot_stack[depth_index] = 0ULL;
        clks_kfree(stack_base);
        return CLKS_TRUE;
    }
#else
    (void)depth_index;
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
        clks_exec_proc_mark_exited(proc, clks_interrupts_timer_ticks(), (u64)-1);
        return CLKS_FALSE;
    }

    if (clks_exec_pid_stack_depth >= CLKS_EXEC_MAX_DEPTH) {
        clks_log(CLKS_LOG_WARN, "EXEC", "PROCESS STACK DEPTH EXCEEDED");
        clks_exec_proc_mark_exited(proc, clks_interrupts_timer_ticks(), (u64)-1);
        return CLKS_FALSE;
    }

    clks_exec_proc_mark_running(proc, clks_interrupts_timer_ticks());

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

    proc->image_mem_bytes = info.total_load_memsz;

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

    clks_exec_log_info_serial("EXEC RUN START");
    clks_exec_log_info_serial(proc->path);
    clks_exec_log_hex_serial("ENTRY", info.entry);
    clks_exec_log_hex_serial("PHNUM", (u64)info.phnum);

    clks_exec_running_depth++;
    depth_counted = CLKS_TRUE;

    if (clks_exec_invoke_entry(entry_ptr, (u32)depth_index, &run_ret) == CLKS_FALSE) {
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

    clks_exec_log_info_serial("RUN RETURNED");
    clks_exec_log_info_serial(proc->path);
    clks_exec_log_hex_serial("RET", run_ret);

    clks_exec_success++;

    clks_exec_proc_mark_exited(proc, clks_interrupts_timer_ticks(), run_ret);

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

    clks_exec_proc_mark_exited(proc, clks_interrupts_timer_ticks(), (u64)-1);

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

static clks_bool clks_exec_run_path_internal(const char *path,
                                             const char *argv_line,
                                             const char *env_line,
                                             u64 *out_status,
                                             u64 *out_pid) {
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
    proc = clks_exec_prepare_proc_record(slot, pid, path, argv_line, env_line, CLKS_EXEC_PROC_RUNNING);

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
    clks_memset(clks_exec_unwind_slot_stack, 0, sizeof(clks_exec_unwind_slot_stack));
    clks_memset(clks_exec_unwind_slot_valid_stack, 0, sizeof(clks_exec_unwind_slot_valid_stack));
    clks_memset(clks_exec_proc_table, 0, sizeof(clks_exec_proc_table));
    clks_exec_log_info_serial("PATH EXEC FRAMEWORK ONLINE");
}

clks_bool clks_exec_run_path(const char *path, u64 *out_status) {
    return clks_exec_run_path_internal(path, CLKS_NULL, CLKS_NULL, out_status, CLKS_NULL);
}

clks_bool clks_exec_run_pathv(const char *path, const char *argv_line, const char *env_line, u64 *out_status) {
    return clks_exec_run_path_internal(path, argv_line, env_line, out_status, CLKS_NULL);
}

clks_bool clks_exec_spawn_pathv(const char *path, const char *argv_line, const char *env_line, u64 *out_pid) {
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
    proc = clks_exec_prepare_proc_record(slot, pid, path, argv_line, env_line, CLKS_EXEC_PROC_PENDING);

    if (proc == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (out_pid != CLKS_NULL) {
        *out_pid = pid;
    }

    clks_exec_log_info_serial("SPAWN QUEUED");
    clks_exec_log_info_serial(path);
    clks_exec_log_hex_serial("PID", pid);

    return CLKS_TRUE;
}

clks_bool clks_exec_spawn_path(const char *path, u64 *out_pid) {
    return clks_exec_spawn_pathv(path, CLKS_NULL, CLKS_NULL, out_pid);
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

    if (proc->state == CLKS_EXEC_PROC_PENDING ||
        proc->state == CLKS_EXEC_PROC_RUNNING ||
        proc->state == CLKS_EXEC_PROC_STOPPED) {
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

static struct clks_exec_proc_record *clks_exec_current_proc(void) {
    i32 depth_index = clks_exec_current_depth_index();
    i32 slot;

    if (depth_index < 0) {
        return CLKS_NULL;
    }

    slot = clks_exec_proc_find_slot_by_pid(clks_exec_pid_stack[(u32)depth_index]);

    if (slot < 0) {
        return CLKS_NULL;
    }

    if (clks_exec_proc_table[(u32)slot].used == CLKS_FALSE) {
        return CLKS_NULL;
    }

    return &clks_exec_proc_table[(u32)slot];
}

u64 clks_exec_current_argc(void) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();

    if (proc == CLKS_NULL) {
        return 0ULL;
    }

    return (u64)proc->argc;
}

clks_bool clks_exec_copy_current_argv(u64 index, char *out_value, usize out_size) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();

    if (proc == CLKS_NULL || out_value == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    if (index >= (u64)proc->argc || index >= CLKS_EXEC_MAX_ARGS) {
        return CLKS_FALSE;
    }

    clks_exec_copy_line(out_value, out_size, proc->argv_items[(u32)index]);
    return CLKS_TRUE;
}

u64 clks_exec_current_envc(void) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();

    if (proc == CLKS_NULL) {
        return 0ULL;
    }

    return (u64)proc->envc;
}

clks_bool clks_exec_copy_current_env(u64 index, char *out_value, usize out_size) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();

    if (proc == CLKS_NULL || out_value == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    if (index >= (u64)proc->envc || index >= CLKS_EXEC_MAX_ENVS) {
        return CLKS_FALSE;
    }

    clks_exec_copy_line(out_value, out_size, proc->env_items[(u32)index]);
    return CLKS_TRUE;
}

u64 clks_exec_current_signal(void) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();
    return (proc != CLKS_NULL) ? proc->last_signal : 0ULL;
}

u64 clks_exec_current_fault_vector(void) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();
    return (proc != CLKS_NULL) ? proc->last_fault_vector : 0ULL;
}

u64 clks_exec_current_fault_error(void) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();
    return (proc != CLKS_NULL) ? proc->last_fault_error : 0ULL;
}

u64 clks_exec_current_fault_rip(void) {
    const struct clks_exec_proc_record *proc = clks_exec_current_proc();
    return (proc != CLKS_NULL) ? proc->last_fault_rip : 0ULL;
}

static u64 clks_exec_proc_runtime_ticks(const struct clks_exec_proc_record *proc, u64 now_tick) {
    u64 runtime;

    if (proc == CLKS_NULL || proc->started_tick == 0ULL) {
        return 0ULL;
    }

    runtime = proc->runtime_ticks_accum;

    if (proc->state == CLKS_EXEC_PROC_RUNNING &&
        proc->run_started_tick != 0ULL &&
        now_tick > proc->run_started_tick) {
        runtime += (now_tick - proc->run_started_tick);
    }

    return runtime;
}

static u64 clks_exec_proc_memory_bytes(const struct clks_exec_proc_record *proc) {
    u64 mem = 0ULL;

    if (proc == CLKS_NULL) {
        return 0ULL;
    }

    mem += proc->image_mem_bytes;

    if (proc->state == CLKS_EXEC_PROC_RUNNING) {
        mem += CLKS_EXEC_RUN_STACK_BYTES;
    }

    return mem;
}

u64 clks_exec_proc_count(void) {
    u64 count = 0ULL;
    u32 i;

    for (i = 0U; i < CLKS_EXEC_MAX_PROCS; i++) {
        if (clks_exec_proc_table[i].used == CLKS_TRUE) {
            count++;
        }
    }

    return count;
}

clks_bool clks_exec_proc_pid_at(u64 index, u64 *out_pid) {
    u64 pos = 0ULL;
    u32 i;

    if (out_pid != CLKS_NULL) {
        *out_pid = 0ULL;
    }

    if (out_pid == CLKS_NULL) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < CLKS_EXEC_MAX_PROCS; i++) {
        if (clks_exec_proc_table[i].used != CLKS_TRUE) {
            continue;
        }

        if (pos == index) {
            *out_pid = clks_exec_proc_table[i].pid;
            return CLKS_TRUE;
        }

        pos++;
    }

    return CLKS_FALSE;
}

clks_bool clks_exec_proc_snapshot(u64 pid, struct clks_exec_proc_snapshot *out_snapshot) {
    i32 slot;
    const struct clks_exec_proc_record *proc;
    u64 now_tick;

    if (out_snapshot == CLKS_NULL) {
        return CLKS_FALSE;
    }

    slot = clks_exec_proc_find_slot_by_pid(pid);

    if (slot < 0) {
        return CLKS_FALSE;
    }

    proc = &clks_exec_proc_table[(u32)slot];

    if (proc->used != CLKS_TRUE) {
        return CLKS_FALSE;
    }

    now_tick = clks_interrupts_timer_ticks();
    clks_memset(out_snapshot, 0, sizeof(*out_snapshot));

    out_snapshot->pid = proc->pid;
    out_snapshot->ppid = proc->ppid;
    out_snapshot->state = (u64)proc->state;
    out_snapshot->started_tick = proc->started_tick;
    out_snapshot->exited_tick = proc->exited_tick;
    out_snapshot->exit_status = proc->exit_status;
    out_snapshot->runtime_ticks = clks_exec_proc_runtime_ticks(proc, now_tick);
    out_snapshot->mem_bytes = clks_exec_proc_memory_bytes(proc);
    out_snapshot->tty_index = (u64)proc->tty_index;
    out_snapshot->last_signal = proc->last_signal;
    out_snapshot->last_fault_vector = proc->last_fault_vector;
    out_snapshot->last_fault_error = proc->last_fault_error;
    out_snapshot->last_fault_rip = proc->last_fault_rip;
    clks_exec_copy_path(out_snapshot->path, sizeof(out_snapshot->path), proc->path);

    return CLKS_TRUE;
}

u64 clks_exec_proc_kill(u64 pid, u64 signal) {
    i32 slot;
    struct clks_exec_proc_record *proc;
    u64 effective_signal;
    u64 status;
    u64 now_tick;

    if (pid == 0ULL) {
        return (u64)-1;
    }

    slot = clks_exec_proc_find_slot_by_pid(pid);

    if (slot < 0) {
        return (u64)-1;
    }

    proc = &clks_exec_proc_table[(u32)slot];

    if (proc->used != CLKS_TRUE) {
        return (u64)-1;
    }

    effective_signal = (signal == 0ULL) ? CLKS_EXEC_DEFAULT_KILL_SIGNAL : (signal & 0xFFULL);
    status = clks_exec_encode_signal_status(effective_signal, 0ULL, 0ULL);
    now_tick = clks_interrupts_timer_ticks();

    if (proc->state == CLKS_EXEC_PROC_EXITED) {
        return 1ULL;
    }

    proc->last_signal = effective_signal;
    proc->last_fault_vector = 0ULL;
    proc->last_fault_error = 0ULL;
    proc->last_fault_rip = 0ULL;

    if (effective_signal == CLKS_EXEC_SIGNAL_CONT) {
        if (proc->state == CLKS_EXEC_PROC_STOPPED) {
            proc->state = CLKS_EXEC_PROC_PENDING;
        }
        return 1ULL;
    }

    if (effective_signal == CLKS_EXEC_SIGNAL_STOP) {
        if (proc->state == CLKS_EXEC_PROC_PENDING) {
            proc->state = CLKS_EXEC_PROC_STOPPED;
            return 1ULL;
        }

        if (proc->state == CLKS_EXEC_PROC_STOPPED) {
            return 1ULL;
        }

        return 0ULL;
    }

    if (proc->state == CLKS_EXEC_PROC_PENDING || proc->state == CLKS_EXEC_PROC_STOPPED) {
        clks_exec_proc_mark_exited(proc, now_tick, status);
        return 1ULL;
    }

    if (proc->state == CLKS_EXEC_PROC_RUNNING) {
        i32 depth_index = clks_exec_current_depth_index();

        if (depth_index < 0 || clks_exec_current_pid() != pid) {
            return 0ULL;
        }

        clks_exec_exit_requested_stack[(u32)depth_index] = CLKS_TRUE;
        clks_exec_exit_status_stack[(u32)depth_index] = status;
        return 1ULL;
    }

    return 0ULL;
}

clks_bool clks_exec_handle_exception(u64 vector,
                                     u64 error_code,
                                     u64 rip,
                                     u64 *io_rip,
                                     u64 *io_rdi,
                                     u64 *io_rsi) {
    i32 depth_index;
    struct clks_exec_proc_record *proc;
    u64 signal;
    u64 status;

    if (clks_exec_is_running() == CLKS_FALSE || clks_exec_current_path_is_user() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    depth_index = clks_exec_current_depth_index();
    proc = clks_exec_current_proc();

    if (depth_index < 0 || proc == CLKS_NULL) {
        return CLKS_FALSE;
    }

    signal = clks_exec_signal_from_vector(vector);
    status = clks_exec_encode_signal_status(signal, vector, error_code);

    proc->last_signal = signal;
    proc->last_fault_vector = vector;
    proc->last_fault_error = error_code;
    proc->last_fault_rip = rip;

    clks_exec_exit_requested_stack[(u32)depth_index] = CLKS_TRUE;
    clks_exec_exit_status_stack[(u32)depth_index] = status;

    clks_exec_log_info_serial("USER EXCEPTION CAPTURED");
    clks_exec_log_info_serial(proc->path);
    clks_exec_log_hex_serial("PID", proc->pid);
    clks_exec_log_hex_serial("SIGNAL", signal);
    clks_exec_log_hex_serial("VECTOR", vector);
    clks_exec_log_hex_serial("ERROR", error_code);
    clks_exec_log_hex_serial("RIP", rip);

#if defined(CLKS_ARCH_X86_64)
    if (io_rip == CLKS_NULL || io_rdi == CLKS_NULL || io_rsi == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_exec_unwind_slot_valid_stack[(u32)depth_index] == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    *io_rip = (u64)clks_exec_abort_to_caller_x86_64;
    *io_rdi = clks_exec_unwind_slot_stack[(u32)depth_index];
    *io_rsi = status;
    return CLKS_TRUE;
#else
    (void)io_rip;
    (void)io_rdi;
    (void)io_rsi;
    return CLKS_FALSE;
#endif
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

