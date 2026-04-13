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
#define CLEONOS_SYSCALL_TTY_COUNT           21ULL
#define CLEONOS_SYSCALL_TTY_ACTIVE          22ULL
#define CLEONOS_SYSCALL_TTY_SWITCH          23ULL
#define CLEONOS_SYSCALL_TTY_WRITE           24ULL
#define CLEONOS_SYSCALL_TTY_WRITE_CHAR      25ULL
#define CLEONOS_SYSCALL_KBD_GET_CHAR        26ULL
#define CLEONOS_SYSCALL_FS_STAT_TYPE        27ULL
#define CLEONOS_SYSCALL_FS_STAT_SIZE        28ULL
#define CLEONOS_SYSCALL_FS_MKDIR            29ULL
#define CLEONOS_SYSCALL_FS_WRITE            30ULL
#define CLEONOS_SYSCALL_FS_APPEND           31ULL
#define CLEONOS_SYSCALL_FS_REMOVE           32ULL
#define CLEONOS_SYSCALL_LOG_JOURNAL_COUNT   33ULL
#define CLEONOS_SYSCALL_LOG_JOURNAL_READ    34ULL
#define CLEONOS_SYSCALL_KBD_BUFFERED        35ULL
#define CLEONOS_SYSCALL_KBD_PUSHED          36ULL
#define CLEONOS_SYSCALL_KBD_POPPED          37ULL
#define CLEONOS_SYSCALL_KBD_DROPPED         38ULL
#define CLEONOS_SYSCALL_KBD_HOTKEY_SWITCHES 39ULL
#define CLEONOS_SYSCALL_GETPID              40ULL
#define CLEONOS_SYSCALL_SPAWN_PATH          41ULL
#define CLEONOS_SYSCALL_WAITPID             42ULL
#define CLEONOS_SYSCALL_EXIT                43ULL
#define CLEONOS_SYSCALL_SLEEP_TICKS         44ULL
#define CLEONOS_SYSCALL_YIELD               45ULL

u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2);
u64 cleonos_sys_log_write(const char *message, u64 length);
u64 cleonos_sys_timer_ticks(void);
u64 cleonos_sys_task_count(void);
u64 cleonos_sys_service_count(void);
u64 cleonos_sys_service_ready_count(void);
u64 cleonos_sys_context_switches(void);
u64 cleonos_sys_kelf_count(void);
u64 cleonos_sys_kelf_runs(void);
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
u64 cleonos_sys_tty_count(void);
u64 cleonos_sys_tty_active(void);
u64 cleonos_sys_tty_switch(u64 tty_index);
u64 cleonos_sys_tty_write(const char *text, u64 length);
u64 cleonos_sys_tty_write_char(char ch);
u64 cleonos_sys_kbd_get_char(void);
u64 cleonos_sys_fs_stat_type(const char *path);
u64 cleonos_sys_fs_stat_size(const char *path);
u64 cleonos_sys_fs_mkdir(const char *path);
u64 cleonos_sys_fs_write(const char *path, const char *data, u64 size);
u64 cleonos_sys_fs_append(const char *path, const char *data, u64 size);
u64 cleonos_sys_fs_remove(const char *path);
u64 cleonos_sys_log_journal_count(void);
u64 cleonos_sys_log_journal_read(u64 index_from_oldest, char *out_line, u64 out_size);
u64 cleonos_sys_kbd_buffered(void);
u64 cleonos_sys_kbd_pushed(void);
u64 cleonos_sys_kbd_popped(void);
u64 cleonos_sys_kbd_dropped(void);
u64 cleonos_sys_kbd_hotkey_switches(void);
u64 cleonos_sys_getpid(void);
u64 cleonos_sys_spawn_path(const char *path);
u64 cleonos_sys_wait_pid(u64 pid, u64 *out_status);
u64 cleonos_sys_exit(u64 status);
u64 cleonos_sys_sleep_ticks(u64 ticks);
u64 cleonos_sys_yield(void);

#endif