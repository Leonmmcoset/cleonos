#include "cmd_runtime.h"
static int ush_cmd_memstat(void) {
    ush_writeln("memstat (user ABI limited):");
    ush_print_kv_hex("  SERVICE_COUNT", cleonos_sys_service_count());
    ush_print_kv_hex("  SERVICE_READY_COUNT", cleonos_sys_service_ready_count());
    ush_print_kv_hex("  KELF_COUNT", cleonos_sys_kelf_count());
    ush_print_kv_hex("  KELF_RUNS", cleonos_sys_kelf_runs());
    return 1;
}

static int ush_cmd_fsstat(void) {
    ush_writeln("fsstat:");
    ush_print_kv_hex("  NODE_COUNT", cleonos_sys_fs_node_count());
    ush_print_kv_hex("  ROOT_CHILDREN", cleonos_sys_fs_child_count("/"));
    ush_print_kv_hex("  SYSTEM_CHILDREN", cleonos_sys_fs_child_count("/system"));
    ush_print_kv_hex("  SHELL_CHILDREN", cleonos_sys_fs_child_count("/shell"));
    ush_print_kv_hex("  TEMP_CHILDREN", cleonos_sys_fs_child_count("/temp"));
    ush_print_kv_hex("  DRIVER_CHILDREN", cleonos_sys_fs_child_count("/driver"));
    ush_print_kv_hex("  DEV_CHILDREN", cleonos_sys_fs_child_count("/dev"));
    return 1;
}

static int ush_cmd_taskstat(void) {
    ush_writeln("taskstat:");
    ush_print_kv_hex("  TASK_COUNT", cleonos_sys_task_count());
    ush_print_kv_hex("  CURRENT_TASK", cleonos_syscall(CLEONOS_SYSCALL_CUR_TASK, 0ULL, 0ULL, 0ULL));
    ush_print_kv_hex("  TIMER_TICKS", cleonos_sys_timer_ticks());
    ush_print_kv_hex("  CONTEXT_SWITCHES", cleonos_sys_context_switches());
    return 1;
}

static int ush_cmd_userstat(void) {
    ush_writeln("userstat:");
    ush_print_kv_hex("  USER_SHELL_READY", cleonos_sys_user_shell_ready());
    ush_print_kv_hex("  USER_EXEC_REQUESTED", cleonos_sys_user_exec_requested());
    ush_print_kv_hex("  USER_LAUNCH_TRIES", cleonos_sys_user_launch_tries());
    ush_print_kv_hex("  USER_LAUNCH_OK", cleonos_sys_user_launch_ok());
    ush_print_kv_hex("  USER_LAUNCH_FAIL", cleonos_sys_user_launch_fail());
    ush_print_kv_hex("  EXEC_REQUESTS", cleonos_sys_exec_request_count());
    ush_print_kv_hex("  EXEC_SUCCESS", cleonos_sys_exec_success_count());
    ush_print_kv_hex("  TTY_COUNT", cleonos_sys_tty_count());
    ush_print_kv_hex("  TTY_ACTIVE", cleonos_sys_tty_active());
    return 1;
}

static int ush_cmd_kbdstat(void) {
    ush_writeln("kbdstat:");
    ush_print_kv_hex("  BUFFERED", cleonos_sys_kbd_buffered());
    ush_print_kv_hex("  PUSHED", cleonos_sys_kbd_pushed());
    ush_print_kv_hex("  POPPED", cleonos_sys_kbd_popped());
    ush_print_kv_hex("  DROPPED", cleonos_sys_kbd_dropped());
    ush_print_kv_hex("  HOTKEY_SWITCHES", cleonos_sys_kbd_hotkey_switches());
    return 1;
}

static int ush_cmd_shstat(const ush_state *sh) {
    ush_writeln("shstat:");
    ush_print_kv_hex("  CMD_TOTAL", sh->cmd_total);
    ush_print_kv_hex("  CMD_OK", sh->cmd_ok);
    ush_print_kv_hex("  CMD_FAIL", sh->cmd_fail);
    ush_print_kv_hex("  CMD_UNKNOWN", sh->cmd_unknown);
    ush_print_kv_hex("  EXIT_REQUESTED", (sh->exit_requested != 0) ? 1ULL : 0ULL);
    ush_print_kv_hex("  EXIT_CODE", sh->exit_code);
    return 1;
}

static int ush_cmd_stats(const ush_state *sh) {
    (void)ush_cmd_memstat();
    (void)ush_cmd_fsstat();
    (void)ush_cmd_taskstat();
    (void)ush_cmd_userstat();
    (void)ush_cmd_kbdstat();
    (void)ush_cmd_shstat(sh);
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "stats") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_stats(&sh);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
