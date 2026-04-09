#ifndef CLKS_TASK_H
#define CLKS_TASK_H

#include <clks/types.h>

#define CLKS_TASK_NAME_MAX 32U

enum clks_task_state {
    CLKS_TASK_UNUSED = 0,
    CLKS_TASK_READY = 1,
    CLKS_TASK_RUNNING = 2,
    CLKS_TASK_BLOCKED = 3
};

struct clks_task_descriptor {
    u32 id;
    char name[CLKS_TASK_NAME_MAX];
    enum clks_task_state state;
    u32 time_slice_ticks;
    u32 remaining_ticks;
    u64 total_ticks;
    u64 switch_count;
};

#endif