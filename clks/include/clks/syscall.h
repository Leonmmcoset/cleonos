#ifndef CLKS_SYSCALL_H
#define CLKS_SYSCALL_H

#include <clks/types.h>

#define CLKS_SYSCALL_LOG_WRITE        0ULL
#define CLKS_SYSCALL_TIMER_TICKS      1ULL
#define CLKS_SYSCALL_TASK_COUNT       2ULL
#define CLKS_SYSCALL_CURRENT_TASK_ID  3ULL

void clks_syscall_init(void);
u64 clks_syscall_dispatch(void *frame_ptr);
u64 clks_syscall_invoke_kernel(u64 id, u64 arg0, u64 arg1, u64 arg2);

#endif