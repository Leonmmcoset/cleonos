#ifndef CLEONOS_SYSCALL_H
#define CLEONOS_SYSCALL_H

typedef unsigned long long u64;
typedef unsigned long long usize;

#define CLEONOS_SYSCALL_LOG_WRITE           0ULL
#define CLEONOS_SYSCALL_TIMER_TICKS         1ULL
#define CLEONOS_SYSCALL_TASK_COUNT          2ULL
#define CLEONOS_SYSCALL_CUR_TASK            3ULL
#define CLEONOS_SYSCALL_SERVICE_COUNT       4ULL
#define CLEONOS_SYSCALL_SERVICE_READY_COUNT 5ULL
#define CLEONOS_SYSCALL_CONTEXT_SWITCHES    6ULL
#define CLEONOS_SYSCALL_KELF_COUNT          7ULL
#define CLEONOS_SYSCALL_KELF_RUNS           8ULL

u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2);
u64 cleonos_sys_log_write(const char *message, u64 length);
u64 cleonos_sys_timer_ticks(void);

#endif
