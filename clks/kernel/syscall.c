#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/interrupts.h>
#include <clks/kelf.h>
#include <clks/log.h>
#include <clks/scheduler.h>
#include <clks/service.h>
#include <clks/string.h>
#include <clks/syscall.h>
#include <clks/types.h>
#include <clks/userland.h>

#define CLKS_SYSCALL_LOG_MAX_LEN  191U
#define CLKS_SYSCALL_PATH_MAX     192U
#define CLKS_SYSCALL_NAME_MAX      96U

struct clks_syscall_frame {
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 rbp;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 vector;
    u64 error_code;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

static clks_bool clks_syscall_ready = CLKS_FALSE;

static clks_bool clks_syscall_copy_user_string(u64 src_addr, char *dst, usize dst_size) {
    const char *src = (const char *)src_addr;
    usize i = 0U;

    if (src == CLKS_NULL || dst == CLKS_NULL || dst_size == 0U) {
        return CLKS_FALSE;
    }

    while (i + 1U < dst_size) {
        char ch = src[i];
        dst[i] = ch;

        if (ch == '\0') {
            return CLKS_TRUE;
        }

        i++;
    }

    dst[dst_size - 1U] = '\0';
    return CLKS_TRUE;
}

static u64 clks_syscall_log_write(u64 arg0, u64 arg1) {
    const char *src = (const char *)arg0;
    u64 len = arg1;
    char buf[CLKS_SYSCALL_LOG_MAX_LEN + 1U];
    u64 i;

    if (src == CLKS_NULL || len == 0ULL) {
        return 0ULL;
    }

    if (len > CLKS_SYSCALL_LOG_MAX_LEN) {
        len = CLKS_SYSCALL_LOG_MAX_LEN;
    }

    for (i = 0ULL; i < len; i++) {
        buf[i] = src[i];
    }

    buf[len] = '\0';
    clks_log(CLKS_LOG_INFO, "SYSCALL", buf);

    return len;
}

static u64 clks_syscall_fs_child_count(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    return clks_fs_count_children(path);
}

static u64 clks_syscall_fs_get_child_name(u64 arg0, u64 arg1, u64 arg2) {
    char path[CLKS_SYSCALL_PATH_MAX];

    if (arg2 == 0ULL) {
        return 0ULL;
    }

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    if (clks_fs_get_child_name(path, arg1, (char *)arg2, (usize)CLKS_SYSCALL_NAME_MAX) == CLKS_FALSE) {
        return 0ULL;
    }

    return 1ULL;
}

static u64 clks_syscall_fs_read(u64 arg0, u64 arg1, u64 arg2) {
    char path[CLKS_SYSCALL_PATH_MAX];
    const void *data;
    u64 file_size = 0ULL;
    u64 copy_len;

    if (arg1 == 0ULL || arg2 == 0ULL) {
        return 0ULL;
    }

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    data = clks_fs_read_all(path, &file_size);

    if (data == CLKS_NULL || file_size == 0ULL) {
        return 0ULL;
    }

    copy_len = (file_size < arg2) ? file_size : arg2;
    clks_memcpy((void *)arg1, data, (usize)copy_len);
    return copy_len;
}

static u64 clks_syscall_exec_path(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];
    u64 status = (u64)-1;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_exec_run_path(path, &status) == CLKS_FALSE) {
        return (u64)-1;
    }

    return status;
}

void clks_syscall_init(void) {
    clks_syscall_ready = CLKS_TRUE;
    clks_log(CLKS_LOG_INFO, "SYSCALL", "INT80 FRAMEWORK ONLINE");
}

u64 clks_syscall_dispatch(void *frame_ptr) {
    struct clks_syscall_frame *frame = (struct clks_syscall_frame *)frame_ptr;
    u64 id;

    if (clks_syscall_ready == CLKS_FALSE || frame == CLKS_NULL) {
        return (u64)-1;
    }

    id = frame->rax;

    switch (id) {
        case CLKS_SYSCALL_LOG_WRITE:
            return clks_syscall_log_write(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_TIMER_TICKS:
            return clks_interrupts_timer_ticks();
        case CLKS_SYSCALL_TASK_COUNT: {
            struct clks_scheduler_stats stats = clks_scheduler_get_stats();
            return stats.task_count;
        }
        case CLKS_SYSCALL_CURRENT_TASK_ID: {
            struct clks_scheduler_stats stats = clks_scheduler_get_stats();
            return stats.current_task_id;
        }
        case CLKS_SYSCALL_SERVICE_COUNT:
            return clks_service_count();
        case CLKS_SYSCALL_SERVICE_READY_COUNT:
            return clks_service_ready_count();
        case CLKS_SYSCALL_CONTEXT_SWITCHES: {
            struct clks_scheduler_stats stats = clks_scheduler_get_stats();
            return stats.context_switch_count;
        }
        case CLKS_SYSCALL_KELF_COUNT:
            return clks_kelf_count();
        case CLKS_SYSCALL_KELF_RUNS:
            return clks_kelf_total_runs();
        case CLKS_SYSCALL_FS_NODE_COUNT:
            return clks_fs_node_count();
        case CLKS_SYSCALL_FS_CHILD_COUNT:
            return clks_syscall_fs_child_count(frame->rbx);
        case CLKS_SYSCALL_FS_GET_CHILD_NAME:
            return clks_syscall_fs_get_child_name(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_FS_READ:
            return clks_syscall_fs_read(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_EXEC_PATH:
            return clks_syscall_exec_path(frame->rbx);
        case CLKS_SYSCALL_EXEC_REQUESTS:
            return clks_exec_request_count();
        case CLKS_SYSCALL_EXEC_SUCCESS:
            return clks_exec_success_count();
        case CLKS_SYSCALL_USER_SHELL_READY:
            return (clks_userland_shell_ready() == CLKS_TRUE) ? 1ULL : 0ULL;
        case CLKS_SYSCALL_USER_EXEC_REQUESTED:
            return (clks_userland_shell_exec_requested() == CLKS_TRUE) ? 1ULL : 0ULL;
        case CLKS_SYSCALL_USER_LAUNCH_TRIES:
            return clks_userland_launch_attempts();
        case CLKS_SYSCALL_USER_LAUNCH_OK:
            return clks_userland_launch_success();
        case CLKS_SYSCALL_USER_LAUNCH_FAIL:
            return clks_userland_launch_failures();
        default:
            return (u64)-1;
    }
}

u64 clks_syscall_invoke_kernel(u64 id, u64 arg0, u64 arg1, u64 arg2) {
    u64 ret;

    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(id), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory"
    );

    return ret;
}
