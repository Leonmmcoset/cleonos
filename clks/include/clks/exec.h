#ifndef CLKS_EXEC_H
#define CLKS_EXEC_H

#include <clks/types.h>

#define CLKS_EXEC_PROC_PATH_MAX 192U

#define CLKS_EXEC_PROC_STATE_UNUSED  0ULL
#define CLKS_EXEC_PROC_STATE_PENDING 1ULL
#define CLKS_EXEC_PROC_STATE_RUNNING 2ULL
#define CLKS_EXEC_PROC_STATE_EXITED  3ULL
#define CLKS_EXEC_PROC_STATE_STOPPED 4ULL

#define CLKS_EXEC_SIGNAL_KILL  9ULL
#define CLKS_EXEC_SIGNAL_TERM 15ULL
#define CLKS_EXEC_SIGNAL_CONT 18ULL
#define CLKS_EXEC_SIGNAL_STOP 19ULL

struct clks_exec_proc_snapshot {
    u64 pid;
    u64 ppid;
    u64 state;
    u64 started_tick;
    u64 exited_tick;
    u64 exit_status;
    u64 runtime_ticks;
    u64 mem_bytes;
    u64 tty_index;
    u64 last_signal;
    u64 last_fault_vector;
    u64 last_fault_error;
    u64 last_fault_rip;
    char path[CLKS_EXEC_PROC_PATH_MAX];
};

void clks_exec_init(void);
clks_bool clks_exec_run_path(const char *path, u64 *out_status);
clks_bool clks_exec_run_pathv(const char *path, const char *argv_line, const char *env_line, u64 *out_status);
clks_bool clks_exec_spawn_path(const char *path, u64 *out_pid);
clks_bool clks_exec_spawn_pathv(const char *path, const char *argv_line, const char *env_line, u64 *out_pid);
u64 clks_exec_wait_pid(u64 pid, u64 *out_status);
clks_bool clks_exec_request_exit(u64 status);
u64 clks_exec_fd_open(const char *path, u64 flags, u64 mode);
u64 clks_exec_fd_read(u64 fd, void *out_buffer, u64 size);
u64 clks_exec_fd_write(u64 fd, const void *buffer, u64 size);
u64 clks_exec_fd_close(u64 fd);
u64 clks_exec_fd_dup(u64 fd);
u64 clks_exec_dl_open(const char *path);
u64 clks_exec_dl_close(u64 handle);
u64 clks_exec_dl_sym(u64 handle, const char *symbol);
u64 clks_exec_current_pid(void);
u32 clks_exec_current_tty(void);
u64 clks_exec_current_argc(void);
clks_bool clks_exec_copy_current_argv(u64 index, char *out_value, usize out_size);
u64 clks_exec_current_envc(void);
clks_bool clks_exec_copy_current_env(u64 index, char *out_value, usize out_size);
u64 clks_exec_current_signal(void);
u64 clks_exec_current_fault_vector(void);
u64 clks_exec_current_fault_error(void);
u64 clks_exec_current_fault_rip(void);
u64 clks_exec_proc_count(void);
clks_bool clks_exec_proc_pid_at(u64 index, u64 *out_pid);
clks_bool clks_exec_proc_snapshot(u64 pid, struct clks_exec_proc_snapshot *out_snapshot);
u64 clks_exec_proc_kill(u64 pid, u64 signal);
u64 clks_exec_force_stop_tty_running_process(u32 tty_index, u64 *out_pid);
clks_bool clks_exec_try_unwind_signaled_process(u64 interrupted_rip, u64 *io_rip, u64 *io_rdi, u64 *io_rsi);
clks_bool clks_exec_handle_exception(u64 vector,
                                     u64 error_code,
                                     u64 rip,
                                     u64 *io_rip,
                                     u64 *io_rdi,
                                     u64 *io_rsi);
u64 clks_exec_sleep_ticks(u64 ticks);
u64 clks_exec_yield(void);
void clks_exec_tick(u64 tick);
u64 clks_exec_request_count(void);
u64 clks_exec_success_count(void);
clks_bool clks_exec_is_running(void);
clks_bool clks_exec_current_path_is_user(void);

#endif
