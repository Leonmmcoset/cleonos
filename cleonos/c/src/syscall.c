#include <cleonos_syscall.h>

u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2) {
    u64 ret;

    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(id), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory"
    );

    return ret;
}

u64 cleonos_sys_log_write(const char *message, u64 length) {
    return cleonos_syscall(CLEONOS_SYSCALL_LOG_WRITE, (u64)message, length, 0ULL);
}

u64 cleonos_sys_timer_ticks(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_TIMER_TICKS, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_task_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_TASK_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_service_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_SERVICE_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_service_ready_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_SERVICE_READY_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_context_switches(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_CONTEXT_SWITCHES, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_kelf_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KELF_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_kelf_runs(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KELF_RUNS, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_node_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_NODE_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_child_count(const char *dir_path) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_CHILD_COUNT, (u64)dir_path, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_get_child_name(const char *dir_path, u64 index, char *out_name) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_GET_CHILD_NAME, (u64)dir_path, index, (u64)out_name);
}

u64 cleonos_sys_fs_read(const char *path, char *out_buffer, u64 buffer_size) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_READ, (u64)path, (u64)out_buffer, buffer_size);
}

u64 cleonos_sys_exec_path(const char *path) {
    return cleonos_syscall(CLEONOS_SYSCALL_EXEC_PATH, (u64)path, 0ULL, 0ULL);
}

u64 cleonos_sys_exec_request_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_EXEC_REQUESTS, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_exec_success_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_EXEC_SUCCESS, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_user_shell_ready(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_USER_SHELL_READY, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_user_exec_requested(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_USER_EXEC_REQUESTED, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_user_launch_tries(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_USER_LAUNCH_TRIES, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_user_launch_ok(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_USER_LAUNCH_OK, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_user_launch_fail(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_USER_LAUNCH_FAIL, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_tty_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_TTY_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_tty_active(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_TTY_ACTIVE, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_tty_switch(u64 tty_index) {
    return cleonos_syscall(CLEONOS_SYSCALL_TTY_SWITCH, tty_index, 0ULL, 0ULL);
}

u64 cleonos_sys_tty_write(const char *text, u64 length) {
    return cleonos_syscall(CLEONOS_SYSCALL_TTY_WRITE, (u64)text, length, 0ULL);
}

u64 cleonos_sys_tty_write_char(char ch) {
    return cleonos_syscall(CLEONOS_SYSCALL_TTY_WRITE_CHAR, (u64)(unsigned char)ch, 0ULL, 0ULL);
}

u64 cleonos_sys_kbd_get_char(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KBD_GET_CHAR, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_stat_type(const char *path) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_STAT_TYPE, (u64)path, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_stat_size(const char *path) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_STAT_SIZE, (u64)path, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_mkdir(const char *path) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_MKDIR, (u64)path, 0ULL, 0ULL);
}

u64 cleonos_sys_fs_write(const char *path, const char *data, u64 size) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_WRITE, (u64)path, (u64)data, size);
}

u64 cleonos_sys_fs_append(const char *path, const char *data, u64 size) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_APPEND, (u64)path, (u64)data, size);
}

u64 cleonos_sys_fs_remove(const char *path) {
    return cleonos_syscall(CLEONOS_SYSCALL_FS_REMOVE, (u64)path, 0ULL, 0ULL);
}

u64 cleonos_sys_log_journal_count(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_LOG_JOURNAL_COUNT, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_log_journal_read(u64 index_from_oldest, char *out_line, u64 out_size) {
    return cleonos_syscall(CLEONOS_SYSCALL_LOG_JOURNAL_READ, index_from_oldest, (u64)out_line, out_size);
}

u64 cleonos_sys_kbd_buffered(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KBD_BUFFERED, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_kbd_pushed(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KBD_PUSHED, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_kbd_popped(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KBD_POPPED, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_kbd_dropped(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KBD_DROPPED, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_kbd_hotkey_switches(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_KBD_HOTKEY_SWITCHES, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_getpid(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_GETPID, 0ULL, 0ULL, 0ULL);
}

u64 cleonos_sys_spawn_path(const char *path) {
    return cleonos_syscall(CLEONOS_SYSCALL_SPAWN_PATH, (u64)path, 0ULL, 0ULL);
}

u64 cleonos_sys_wait_pid(u64 pid, u64 *out_status) {
    return cleonos_syscall(CLEONOS_SYSCALL_WAITPID, pid, (u64)out_status, 0ULL);
}

u64 cleonos_sys_exit(u64 status) {
    return cleonos_syscall(CLEONOS_SYSCALL_EXIT, status, 0ULL, 0ULL);
}

u64 cleonos_sys_sleep_ticks(u64 ticks) {
    return cleonos_syscall(CLEONOS_SYSCALL_SLEEP_TICKS, ticks, 0ULL, 0ULL);
}

u64 cleonos_sys_yield(void) {
    return cleonos_syscall(CLEONOS_SYSCALL_YIELD, 0ULL, 0ULL, 0ULL);
}