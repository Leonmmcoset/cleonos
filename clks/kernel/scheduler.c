#include <clks/log.h>
#include <clks/scheduler.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_SCHED_MAX_TASKS 16U
#define CLKS_SCHED_MIN_SLICE 1U

static struct clks_task_descriptor clks_tasks[CLKS_SCHED_MAX_TASKS];
static u32 clks_task_count = 0;
static u32 clks_current_task = 0;
static u64 clks_total_timer_ticks = 0;
static u64 clks_context_switch_count = 0;

static void clks_sched_copy_name(char *dst, const char *src) {
    u32 i = 0;

    while (i < (CLKS_TASK_NAME_MAX - 1U) && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static u32 clks_sched_next_ready_task(u32 from) {
    u32 i;

    for (i = 1U; i <= clks_task_count; i++) {
        u32 idx = (from + i) % clks_task_count;

        if (clks_tasks[idx].state == CLKS_TASK_READY || clks_tasks[idx].state == CLKS_TASK_RUNNING) {
            return idx;
        }
    }

    return from;
}

void clks_scheduler_init(void) {
    clks_memset(clks_tasks, 0, sizeof(clks_tasks));
    clks_task_count = 0;
    clks_current_task = 0;
    clks_total_timer_ticks = 0;
    clks_context_switch_count = 0;

    clks_scheduler_add_kernel_task("idle", 1U);

    clks_tasks[0].state = CLKS_TASK_RUNNING;
    clks_tasks[0].remaining_ticks = clks_tasks[0].time_slice_ticks;

    clks_log(CLKS_LOG_INFO, "SCHED", "ROUND-ROBIN ONLINE");
}

clks_bool clks_scheduler_add_kernel_task(const char *name, u32 time_slice_ticks) {
    struct clks_task_descriptor *task;

    if (name == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_task_count >= CLKS_SCHED_MAX_TASKS) {
        return CLKS_FALSE;
    }

    if (time_slice_ticks < CLKS_SCHED_MIN_SLICE) {
        time_slice_ticks = CLKS_SCHED_MIN_SLICE;
    }

    task = &clks_tasks[clks_task_count];
    task->id = clks_task_count;
    clks_sched_copy_name(task->name, name);
    task->state = CLKS_TASK_READY;
    task->time_slice_ticks = time_slice_ticks;
    task->remaining_ticks = time_slice_ticks;
    task->total_ticks = 0;
    task->switch_count = 0;

    clks_task_count++;
    return CLKS_TRUE;
}

void clks_scheduler_on_timer_tick(u64 tick) {
    struct clks_task_descriptor *current;

    if (clks_task_count == 0U) {
        return;
    }

    clks_total_timer_ticks = tick;

    current = &clks_tasks[clks_current_task];

    if (current->state == CLKS_TASK_RUNNING || current->state == CLKS_TASK_READY) {
        current->total_ticks++;

        if (current->remaining_ticks > 0U) {
            current->remaining_ticks--;
        }
    }

    if (current->remaining_ticks == 0U) {
        u32 next = clks_sched_next_ready_task(clks_current_task);

        current->remaining_ticks = current->time_slice_ticks;

        if (next != clks_current_task) {
            if (current->state == CLKS_TASK_RUNNING) {
                current->state = CLKS_TASK_READY;
            }

            clks_current_task = next;
            clks_tasks[clks_current_task].state = CLKS_TASK_RUNNING;
            clks_tasks[clks_current_task].switch_count++;
            clks_tasks[clks_current_task].remaining_ticks = clks_tasks[clks_current_task].time_slice_ticks;
            clks_context_switch_count++;
        }
    }
}

struct clks_scheduler_stats clks_scheduler_get_stats(void) {
    struct clks_scheduler_stats stats;

    stats.task_count = clks_task_count;
    stats.current_task_id = clks_current_task;
    stats.total_timer_ticks = clks_total_timer_ticks;
    stats.context_switch_count = clks_context_switch_count;

    return stats;
}

const struct clks_task_descriptor *clks_scheduler_get_task(u32 task_id) {
    if (task_id >= clks_task_count) {
        return CLKS_NULL;
    }

    return &clks_tasks[task_id];
}