#ifndef CLKS_EXEC_H
#define CLKS_EXEC_H

#include <clks/types.h>

void clks_exec_init(void);
clks_bool clks_exec_run_path(const char *path, u64 *out_status);
clks_bool clks_exec_spawn_path(const char *path, u64 *out_pid);
u64 clks_exec_wait_pid(u64 pid, u64 *out_status);
clks_bool clks_exec_request_exit(u64 status);
u64 clks_exec_current_pid(void);
u32 clks_exec_current_tty(void);
u64 clks_exec_sleep_ticks(u64 ticks);
u64 clks_exec_yield(void);
void clks_exec_tick(u64 tick);
u64 clks_exec_request_count(void);
u64 clks_exec_success_count(void);
clks_bool clks_exec_is_running(void);
clks_bool clks_exec_current_path_is_user(void);

#endif
