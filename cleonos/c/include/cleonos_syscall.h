#ifndef CLEONOS_SYSCALL_H
#define CLEONOS_SYSCALL_H

typedef unsigned long long u64;
typedef unsigned long long usize;

#define CLEONOS_FS_NAME_MAX 96ULL

#define CLEONOS_SYSCALL_LOG_WRITE           0ULL
#define CLEONOS_SYSCALL_TIMER_TICKS         1ULL
#define CLEONOS_SYSCALL_TASK_COUNT          2ULL
#define CLEONOS_SYSCALL_CUR_TASK            3ULL
#define CLEONOS_SYSCALL_SERVICE_COUNT       4ULL
#define CLEONOS_SYSCALL_SERVICE_READY_COUNT 5ULL
#define CLEONOS_SYSCALL_CONTEXT_SWITCHES    6ULL
#define CLEONOS_SYSCALL_KELF_COUNT          7ULL
#define CLEONOS_SYSCALL_KELF_RUNS           8ULL
#define CLEONOS_SYSCALL_FS_NODE_COUNT       9ULL
#define CLEONOS_SYSCALL_FS_CHILD_COUNT      10ULL
#define CLEONOS_SYSCALL_FS_GET_CHILD_NAME   11ULL
#define CLEONOS_SYSCALL_FS_READ             12ULL
#define CLEONOS_SYSCALL_EXEC_PATH           13ULL
#define CLEONOS_SYSCALL_EXEC_REQUESTS       14ULL
#define CLEONOS_SYSCALL_EXEC_SUCCESS        15ULL
#define CLEONOS_SYSCALL_USER_SHELL_READY    16ULL
#define CLEONOS_SYSCALL_USER_EXEC_REQUESTED 17ULL
#define CLEONOS_SYSCALL_USER_LAUNCH_TRIES   18ULL
#define CLEONOS_SYSCALL_USER_LAUNCH_OK      19ULL
#define CLEONOS_SYSCALL_USER_LAUNCH_FAIL    20ULL

u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2);
u64 cleonos_sys_log_write(const char *message, u64 length);
u64 cleonos_sys_timer_ticks(void);
u64 cleonos_sys_task_count(void);
u64 cleonos_sys_fs_node_count(void);
u64 cleonos_sys_fs_child_count(const char *dir_path);
u64 cleonos_sys_fs_get_child_name(const char *dir_path, u64 index, char *out_name);
u64 cleonos_sys_fs_read(const char *path, char *out_buffer, u64 buffer_size);
u64 cleonos_sys_exec_path(const char *path);
u64 cleonos_sys_exec_request_count(void);
u64 cleonos_sys_exec_success_count(void);
u64 cleonos_sys_user_shell_ready(void);
u64 cleonos_sys_user_exec_requested(void);
u64 cleonos_sys_user_launch_tries(void);
u64 cleonos_sys_user_launch_ok(void);
u64 cleonos_sys_user_launch_fail(void);

#endif
