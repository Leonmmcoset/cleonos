#include <cleonos_syscall.h>

u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2) {
    u64 ret;

    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(id), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory"
    );

    return ret;
}

u64 cleonos_sys_log_write(const char *message, u64 length) {
    return cleonos_syscall(CLEONOS_SYSCALL_LOG_WRITE, (u64)message, length, 0ULL);
}

u64 cleonos_sys_timer_ticks(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_TIMER_TICKS, 0ULL, 0ULL, 0ULL);
}
