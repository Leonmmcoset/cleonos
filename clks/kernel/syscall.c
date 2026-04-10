#include <clks/interrupts.h>
#include <clks/kelf.h>
#include <clks/log.h>
#include <clks/scheduler.h>
#include <clks/service.h>
#include <clks/syscall.h>
#include <clks/types.h>

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

static u64 clks_syscall_log_write(u64 arg0, u64 arg1) {
    const char *src = (const char *)arg0;
    u64 len = arg1;
    char buf[192];
    u64 i;

    if (src == CLKS_NULL || len == 0ULL) {
        return 0ULL;
    }

    if (len > (sizeof(buf) - 1U)) {
        len = sizeof(buf) - 1U;
    }

    for (i = 0; i < len; i++) {
        buf[i] = src[i];
    }

    buf[len] = '\0';
    clks_log(CLKS_LOG_INFO, "SYSCALL", buf);

    return len;
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
