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
