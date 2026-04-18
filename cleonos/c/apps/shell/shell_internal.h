#ifndef CLEONOS_USER_SHELL_INTERNAL_H
#define CLEONOS_USER_SHELL_INTERNAL_H

#include <cleonos_syscall.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

#define USH_CMD_MAX          32ULL
#define USH_ARG_MAX         160ULL
#define USH_LINE_MAX        192ULL
#define USH_PATH_MAX        192ULL
#define USH_CAT_MAX         512ULL
#define USH_SCRIPT_MAX     1024ULL
#define USH_CLEAR_LINES      56ULL
#define USH_HISTORY_MAX      16ULL

#define USH_KEY_LEFT    ((char)0x01)
#define USH_KEY_RIGHT   ((char)0x02)
#define USH_KEY_UP      ((char)0x03)
#define USH_KEY_DOWN    ((char)0x04)
#define USH_KEY_HOME    ((char)0x05)
#define USH_KEY_END     ((char)0x06)
#define USH_KEY_DELETE  ((char)0x07)
#define USH_KEY_SELECT_ALL ((char)0x10)
#define USH_KEY_COPY       ((char)0x11)
#define USH_KEY_PASTE      ((char)0x12)
#define USH_KEY_SHIFT_LEFT ((char)0x13)
#define USH_KEY_SHIFT_RIGHT ((char)0x14)
#define USH_KEY_SHIFT_HOME ((char)0x15)
#define USH_KEY_SHIFT_END  ((char)0x16)

#define USH_CMD_CTX_PATH "/temp/.ush_cmd_ctx.bin"
#define USH_CMD_RET_PATH "/temp/.ush_cmd_ret.bin"
#define USH_CMD_RET_FLAG_CWD  0x1ULL
#define USH_CMD_RET_FLAG_EXIT 0x2ULL

typedef struct ush_state {
    char line[USH_LINE_MAX];
    u64 line_len;
    u64 cursor;
    u64 rendered_len;

    char cwd[USH_PATH_MAX];

    char history[USH_HISTORY_MAX][USH_LINE_MAX];
    u64 history_count;
    i64 history_nav;
    char nav_saved_line[USH_LINE_MAX];
    u64 nav_saved_len;
    u64 nav_saved_cursor;

    u64 cmd_total;
    u64 cmd_ok;
    u64 cmd_fail;
    u64 cmd_unknown;
    int exit_requested;
    u64 exit_code;
} ush_state;

typedef struct ush_cmd_ctx {
    char cmd[USH_CMD_MAX];
    char arg[USH_ARG_MAX];
    char cwd[USH_PATH_MAX];
} ush_cmd_ctx;

typedef struct ush_cmd_ret {
    u64 flags;
    u64 exit_code;
    char cwd[USH_PATH_MAX];
} ush_cmd_ret;

void ush_init_state(ush_state *sh);

u64 ush_strlen(const char *str);
int ush_streq(const char *left, const char *right);
int ush_is_space(char ch);
int ush_is_printable(char ch);
int ush_has_suffix(const char *name, const char *suffix);
int ush_contains_char(const char *text, char needle);
int ush_parse_u64_dec(const char *text, u64 *out_value);
void ush_copy(char *dst, u64 dst_size, const char *src);
void ush_trim_line(char *line);
void ush_parse_line(const char *line, char *out_cmd, u64 cmd_size, char *out_arg, u64 arg_size);

void ush_write(const char *text);
void ush_write_char(char ch);
void ush_writeln(const char *text);
void ush_output_capture_begin(char *buffer, u64 buffer_size, int mirror_to_tty);
u64 ush_output_capture_end(void);
int ush_output_capture_truncated(void);
void ush_output_fd_begin(u64 fd, int mirror_to_tty);
void ush_output_fd_end(void);
void ush_prompt(const ush_state *sh);
void ush_write_hex_u64(u64 value);
void ush_print_kv_hex(const char *label, u64 value);

int ush_resolve_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size);
int ush_resolve_exec_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size);
int ush_path_is_under_system(const char *path);

void ush_read_line(ush_state *sh, char *out_line, u64 out_size);
int ush_run_script_file(ush_state *sh, const char *path);
void ush_execute_line(ush_state *sh, const char *line);

int ush_command_ctx_write(const char *cmd, const char *arg, const char *cwd);
int ush_command_ctx_read(ush_cmd_ctx *out_ctx);
void ush_command_ret_reset(void);
int ush_command_ret_write(const ush_cmd_ret *ret);
int ush_command_ret_read(ush_cmd_ret *out_ret);
int ush_try_exec_external(ush_state *sh, const char *cmd, const char *arg, int *out_success);
int ush_try_exec_external_with_fds(ush_state *sh,
                                   const char *cmd,
                                   const char *arg,
                                   u64 stdin_fd,
                                   u64 stdout_fd,
                                   u64 stderr_fd,
                                   int *out_success);

#endif

