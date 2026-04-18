#include "shell_internal.h"

static void ush_zero(void *ptr, u64 size) {
    u64 i;
    char *bytes = (char *)ptr;

    if (bytes == (char *)0) {
        return;
    }

    for (i = 0ULL; i < size; i++) {
        bytes[i] = 0;
    }
}

static void ush_append_text(char *dst, u64 dst_size, const char *text) {
    u64 p = 0ULL;
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL || text == (const char *)0) {
        return;
    }

    while (dst[p] != '\0' && p + 1ULL < dst_size) {
        p++;
    }

    while (text[i] != '\0' && p + 1ULL < dst_size) {
        dst[p++] = text[i++];
    }

    dst[p] = '\0';
}

static const char *ush_alias_command(const char *cmd) {
    if (cmd == (const char *)0) {
        return (const char *)0;
    }

    if (ush_streq(cmd, "dir") != 0) {
        return "ls";
    }

    if (ush_streq(cmd, "run") != 0) {
        return "exec";
    }

    if (ush_streq(cmd, "poweroff") != 0) {
        return "shutdown";
    }

    if (ush_streq(cmd, "reboot") != 0) {
        return "restart";
    }

    if (ush_streq(cmd, "cls") != 0) {
        return "clear";
    }

    if (ush_streq(cmd, "color") != 0) {
        return "ansi";
    }

    return cmd;
}

static int ush_cmd_ret_apply(ush_state *sh, const ush_cmd_ret *ret) {
    if (sh == (ush_state *)0 || ret == (const ush_cmd_ret *)0) {
        return 0;
    }

    if ((ret->flags & USH_CMD_RET_FLAG_CWD) != 0ULL && ret->cwd[0] == '/') {
        ush_copy(sh->cwd, (u64)sizeof(sh->cwd), ret->cwd);
    }

    if ((ret->flags & USH_CMD_RET_FLAG_EXIT) != 0ULL) {
        sh->exit_requested = 1;
        sh->exit_code = ret->exit_code;
    }

    return 1;
}

int ush_command_ctx_write(const char *cmd, const char *arg, const char *cwd) {
    ush_cmd_ctx ctx;

    ush_zero(&ctx, (u64)sizeof(ctx));

    if (cmd != (const char *)0) {
        ush_copy(ctx.cmd, (u64)sizeof(ctx.cmd), cmd);
    }

    if (arg != (const char *)0) {
        ush_copy(ctx.arg, (u64)sizeof(ctx.arg), arg);
    }

    if (cwd != (const char *)0) {
        ush_copy(ctx.cwd, (u64)sizeof(ctx.cwd), cwd);
    }

    return (cleonos_sys_fs_write(USH_CMD_CTX_PATH, (const char *)&ctx, (u64)sizeof(ctx)) != 0ULL) ? 1 : 0;
}

int ush_command_ctx_read(ush_cmd_ctx *out_ctx) {
    u64 got;

    if (out_ctx == (ush_cmd_ctx *)0) {
        return 0;
    }

    ush_zero(out_ctx, (u64)sizeof(*out_ctx));
    got = cleonos_sys_fs_read(USH_CMD_CTX_PATH, (char *)out_ctx, (u64)sizeof(*out_ctx));
    return (got == (u64)sizeof(*out_ctx)) ? 1 : 0;
}

void ush_command_ret_reset(void) {
    (void)cleonos_sys_fs_remove(USH_CMD_RET_PATH);
}

int ush_command_ret_write(const ush_cmd_ret *ret) {
    if (ret == (const ush_cmd_ret *)0) {
        return 0;
    }

    return (cleonos_sys_fs_write(USH_CMD_RET_PATH, (const char *)ret, (u64)sizeof(*ret)) != 0ULL) ? 1 : 0;
}

int ush_command_ret_read(ush_cmd_ret *out_ret) {
    u64 got;

    if (out_ret == (ush_cmd_ret *)0) {
        return 0;
    }

    ush_zero(out_ret, (u64)sizeof(*out_ret));
    got = cleonos_sys_fs_read(USH_CMD_RET_PATH, (char *)out_ret, (u64)sizeof(*out_ret));
    return (got == (u64)sizeof(*out_ret)) ? 1 : 0;
}

int ush_try_exec_external_with_fds(ush_state *sh, const char *cmd, const char *arg, u64 stdin_fd, u64 stdout_fd,
                                   u64 stderr_fd, int *out_success) {
    const char *canonical;
    char path[USH_PATH_MAX];
    char env_line[USH_PATH_MAX + USH_CMD_MAX + 96ULL];
    u64 status;
    ush_cmd_ret ret;

    if (out_success != (int *)0) {
        *out_success = 0;
    }

    if (sh == (ush_state *)0 || cmd == (const char *)0 || cmd[0] == '\0') {
        return 0;
    }

    canonical = ush_alias_command(cmd);

    if (canonical == (const char *)0) {
        return 0;
    }

    if (ush_resolve_exec_path(sh, canonical, path, (u64)sizeof(path)) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_stat_type(path) != 1ULL) {
        return 0;
    }

    ush_command_ret_reset();

    if (ush_command_ctx_write(canonical, arg, sh->cwd) == 0) {
        ush_writeln("exec: command context write failed");
        return 1;
    }

    env_line[0] = '\0';
    ush_append_text(env_line, (u64)sizeof(env_line), "PWD=");
    ush_append_text(env_line, (u64)sizeof(env_line), sh->cwd);
    ush_append_text(env_line, (u64)sizeof(env_line), ";CMD=");
    ush_append_text(env_line, (u64)sizeof(env_line), canonical);

    if (stdin_fd != CLEONOS_FD_INHERIT) {
        ush_append_text(env_line, (u64)sizeof(env_line), ";USH_STDIN_MODE=PIPE");
    }

    status = cleonos_sys_exec_pathv_io(path, arg, env_line, stdin_fd, stdout_fd, stderr_fd);

    if (status == (u64)-1) {
        ush_writeln("exec: request failed");
        (void)cleonos_sys_fs_remove(USH_CMD_CTX_PATH);
        return 1;
    }

    if (ush_command_ret_read(&ret) != 0) {
        (void)ush_cmd_ret_apply(sh, &ret);
    }

    (void)cleonos_sys_fs_remove(USH_CMD_CTX_PATH);
    (void)cleonos_sys_fs_remove(USH_CMD_RET_PATH);

    if (status != 0ULL) {
        if ((status & (1ULL << 63)) != 0ULL) {
            ush_writeln("exec: terminated by signal");
            ush_print_kv_hex("  SIGNAL", status & 0xFFULL);
            ush_print_kv_hex("  VECTOR", (status >> 8) & 0xFFULL);
            ush_print_kv_hex("  ERROR", (status >> 16) & 0xFFFFULL);
        } else {
            ush_writeln("exec: returned non-zero status");
            ush_print_kv_hex("  STATUS", status);
        }

        if (out_success != (int *)0) {
            *out_success = 0;
        }

        return 1;
    }

    if (out_success != (int *)0) {
        *out_success = 1;
    }

    return 1;
}

int ush_try_exec_external(ush_state *sh, const char *cmd, const char *arg, int *out_success) {
    return ush_try_exec_external_with_fds(sh, cmd, arg, CLEONOS_FD_INHERIT, CLEONOS_FD_INHERIT, CLEONOS_FD_INHERIT,
                                          out_success);
}
