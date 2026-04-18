#include <clks/elf64.h>
#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/log.h>
#include <clks/types.h>
#include <clks/userland.h>

#define CLKS_USERLAND_RETRY_INTERVAL 500ULL

#ifndef CLKS_CFG_USERLAND_AUTO_EXEC
#define CLKS_CFG_USERLAND_AUTO_EXEC 1
#endif

static clks_bool clks_user_shell_ready = CLKS_FALSE;
static clks_bool clks_user_shell_exec_requested_flag = CLKS_FALSE;
static clks_bool clks_user_shell_exec_enabled = CLKS_FALSE;
static u64 clks_user_launch_attempt_count = 0ULL;
static u64 clks_user_launch_success_count = 0ULL;
static u64 clks_user_launch_fail_count = 0ULL;
static u64 clks_user_last_try_tick = 0ULL;
static clks_bool clks_user_first_try_pending = CLKS_FALSE;

static clks_bool clks_userland_probe_elf(const char *path, const char *tag) {
    const void *image;
    u64 size = 0ULL;
    struct clks_elf64_info info;

    image = clks_fs_read_all(path, &size);

    if (image == CLKS_NULL) {
        clks_log(CLKS_LOG_ERROR, "USER", "ELF FILE MISSING");
        clks_log(CLKS_LOG_ERROR, "USER", path);
        return CLKS_FALSE;
    }

    if (clks_elf64_inspect(image, size, &info) == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "USER", "ELF INSPECT FAILED");
        clks_log(CLKS_LOG_ERROR, "USER", path);
        return CLKS_FALSE;
    }

    clks_log(CLKS_LOG_INFO, "USER", tag);
    clks_log_hex(CLKS_LOG_INFO, "USER", "ELF_SIZE", size);
    clks_log_hex(CLKS_LOG_INFO, "USER", "ENTRY", info.entry);
    return CLKS_TRUE;
}

static void clks_userland_probe_init_script(void) {
    const void *data;
    u64 size = 0ULL;

    data = clks_fs_read_all("/shell/init.cmd", &size);

    if (data == CLKS_NULL || size == 0ULL) {
        clks_log(CLKS_LOG_WARN, "USER", "INIT SCRIPT NOT FOUND /SHELL/INIT.CMD");
        return;
    }

    clks_log(CLKS_LOG_INFO, "USER", "INIT SCRIPT READY /SHELL/INIT.CMD");
    clks_log_hex(CLKS_LOG_INFO, "USER", "INIT_SCRIPT_SIZE", size);
}

static clks_bool clks_userland_request_shell_exec(void) {
    u64 status = (u64)-1;

    if (clks_user_shell_ready == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_user_launch_attempt_count++;

    if (clks_exec_run_path("/shell/shell.elf", &status) == CLKS_TRUE && status == 0ULL) {
        clks_user_shell_exec_requested_flag = CLKS_TRUE;
        clks_user_launch_success_count++;

        clks_log(CLKS_LOG_INFO, "USER", "SHELL EXEC REQUESTED");
        clks_log_hex(CLKS_LOG_INFO, "USER", "SHELL_STATUS", status);
        return CLKS_TRUE;
    }

    clks_user_launch_fail_count++;
    clks_log(CLKS_LOG_WARN, "USER", "SHELL EXEC REQUEST FAILED");
    return CLKS_FALSE;
}

clks_bool clks_userland_init(void) {
    clks_log(CLKS_LOG_INFO, "USER", "USERLAND FRAMEWORK ONLINE");

    clks_user_shell_ready = CLKS_FALSE;
    clks_user_shell_exec_requested_flag = CLKS_FALSE;
    clks_user_shell_exec_enabled = (CLKS_CFG_USERLAND_AUTO_EXEC != 0) ? CLKS_TRUE : CLKS_FALSE;
    clks_user_launch_attempt_count = 0ULL;
    clks_user_launch_success_count = 0ULL;
    clks_user_launch_fail_count = 0ULL;
    clks_user_last_try_tick = 0ULL;
    clks_user_first_try_pending = CLKS_TRUE;

    if (clks_userland_probe_elf("/shell/shell.elf", "SHELL ELF READY") == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_user_shell_ready = CLKS_TRUE;
    clks_log(CLKS_LOG_INFO, "USER", "SHELL COMMAND ABI READY");
    clks_userland_probe_init_script();

    if (clks_userland_probe_elf("/system/elfrunner.elf", "ELFRUNNER ELF READY") == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_userland_probe_elf("/system/memc.elf", "MEMC ELF READY") == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_user_shell_exec_enabled == CLKS_TRUE) {
        clks_log(CLKS_LOG_INFO, "USER", "USER SHELL AUTO EXEC ENABLED");
    } else {
        clks_log(CLKS_LOG_WARN, "USER", "USER SHELL AUTO EXEC DISABLED BY MENUCONFIG");
    }
    return CLKS_TRUE;
}

void clks_userland_tick(u64 tick) {
    if (clks_user_shell_exec_enabled == CLKS_FALSE ||
        clks_user_shell_ready == CLKS_FALSE ||
        clks_user_shell_exec_requested_flag == CLKS_TRUE) {
        return;
    }

    if (clks_user_first_try_pending == CLKS_TRUE) {
        clks_user_first_try_pending = CLKS_FALSE;
        clks_user_last_try_tick = tick;
        (void)clks_userland_request_shell_exec();
        return;
    }

    if (tick - clks_user_last_try_tick < CLKS_USERLAND_RETRY_INTERVAL) {
        return;
    }

    clks_user_last_try_tick = tick;
    (void)clks_userland_request_shell_exec();
}

clks_bool clks_userland_shell_ready(void) {
    return clks_user_shell_ready;
}

clks_bool clks_userland_shell_exec_requested(void) {
    return clks_user_shell_exec_requested_flag;
}

clks_bool clks_userland_shell_auto_exec_enabled(void) {
    return clks_user_shell_exec_enabled;
}

u64 clks_userland_launch_attempts(void) {
    return clks_user_launch_attempt_count;
}

u64 clks_userland_launch_success(void) {
    return clks_user_launch_success_count;
}

u64 clks_userland_launch_failures(void) {
    return clks_user_launch_fail_count;
}
