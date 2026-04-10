#ifndef CLKS_USERLAND_H
#define CLKS_USERLAND_H

#include <clks/types.h>

clks_bool clks_userland_init(void);
void clks_userland_tick(u64 tick);
clks_bool clks_userland_shell_ready(void);
clks_bool clks_userland_shell_exec_requested(void);
u64 clks_userland_launch_attempts(void);
u64 clks_userland_launch_success(void);
u64 clks_userland_launch_failures(void);

#endif
