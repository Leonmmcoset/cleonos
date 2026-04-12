#ifndef CLKS_EXEC_H
#define CLKS_EXEC_H

#include <clks/types.h>

void clks_exec_init(void);
clks_bool clks_exec_run_path(const char *path, u64 *out_status);
u64 clks_exec_request_count(void);
u64 clks_exec_success_count(void);
clks_bool clks_exec_is_running(void);

#endif

