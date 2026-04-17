#include <clks/cpu.h>
#include <clks/audio.h>
#include <clks/exec.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/interrupts.h>
#include <clks/kelf.h>
#include <clks/keyboard.h>
#include <clks/log.h>
#include <clks/serial.h>
#include <clks/scheduler.h>
#include <clks/service.h>
#include <clks/string.h>
#include <clks/syscall.h>
#include <clks/tty.h>
#include <clks/types.h>
#include <clks/userland.h>

#define CLKS_SYSCALL_LOG_MAX_LEN      191U
#define CLKS_SYSCALL_PATH_MAX         192U
#define CLKS_SYSCALL_NAME_MAX          96U
#define CLKS_SYSCALL_TTY_MAX_LEN      512U
#define CLKS_SYSCALL_FS_IO_MAX_LEN  65536U
#define CLKS_SYSCALL_JOURNAL_MAX_LEN  256U
#define CLKS_SYSCALL_ARG_LINE_MAX     256U
#define CLKS_SYSCALL_ENV_LINE_MAX     512U
#define CLKS_SYSCALL_ITEM_MAX         128U
#define CLKS_SYSCALL_PROCFS_TEXT_MAX 2048U
#define CLKS_SYSCALL_USER_TRACE_BUDGET 128ULL

struct clks_syscall_frame {
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 rbp;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 vector;
    u64 error_code;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

static clks_bool clks_syscall_ready = CLKS_FALSE;
static clks_bool clks_syscall_user_trace_active = CLKS_FALSE;
static u64 clks_syscall_user_trace_budget = 0ULL;

#if defined(CLKS_ARCH_X86_64)
static inline void clks_syscall_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void clks_syscall_outw(u16 port, u16 value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
#endif


static clks_bool clks_syscall_copy_user_string(u64 src_addr, char *dst, usize dst_size) {
    const char *src = (const char *)src_addr;
    usize i = 0U;

    if (src == CLKS_NULL || dst == CLKS_NULL || dst_size == 0U) {
        return CLKS_FALSE;
    }

    while (i + 1U < dst_size) {
        char ch = src[i];
        dst[i] = ch;

        if (ch == '\0') {
            return CLKS_TRUE;
        }

        i++;
    }

    dst[dst_size - 1U] = '\0';
    return CLKS_TRUE;
}

static clks_bool clks_syscall_copy_user_optional_string(u64 src_addr, char *dst, usize dst_size) {
    if (dst == CLKS_NULL || dst_size == 0U) {
        return CLKS_FALSE;
    }

    if (src_addr == 0ULL) {
        dst[0] = '\0';
        return CLKS_TRUE;
    }

    return clks_syscall_copy_user_string(src_addr, dst, dst_size);
}

static u64 clks_syscall_log_write(u64 arg0, u64 arg1) {
    const char *src = (const char *)arg0;
    u64 len = arg1;
    char buf[CLKS_SYSCALL_LOG_MAX_LEN + 1U];
    u64 i;

    if (src == CLKS_NULL || len == 0ULL) {
        return 0ULL;
    }

    if (len > CLKS_SYSCALL_LOG_MAX_LEN) {
        len = CLKS_SYSCALL_LOG_MAX_LEN;
    }

    for (i = 0ULL; i < len; i++) {
        buf[i] = src[i];
    }

    buf[len] = '\0';
    clks_log(CLKS_LOG_INFO, "SYSCALL", buf);

    return len;
}

static u64 clks_syscall_tty_write(u64 arg0, u64 arg1) {
    const char *src = (const char *)arg0;
    u64 len = arg1;
    char buf[CLKS_SYSCALL_TTY_MAX_LEN + 1U];
    u64 i;

    if (src == CLKS_NULL || len == 0ULL) {
        return 0ULL;
    }

    if (len > CLKS_SYSCALL_TTY_MAX_LEN) {
        len = CLKS_SYSCALL_TTY_MAX_LEN;
    }

    for (i = 0ULL; i < len; i++) {
        buf[i] = src[i];
    }

    buf[len] = '\0';
    clks_tty_write(buf);
    return len;
}

static u64 clks_syscall_tty_write_char(u64 arg0) {
    clks_tty_write_char((char)(arg0 & 0xFFULL));
    return 1ULL;
}

static u64 clks_syscall_kbd_get_char(void) {
    char ch;
    u32 tty_index = clks_exec_current_tty();

    if (clks_keyboard_pop_char_for_tty(tty_index, &ch) == CLKS_FALSE) {
        return (u64)-1;
    }

    return (u64)(u8)ch;
}

static clks_bool clks_syscall_procfs_is_root(const char *path) {
    return (path != CLKS_NULL && clks_strcmp(path, "/proc") == 0) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_syscall_procfs_is_self(const char *path) {
    return (path != CLKS_NULL && clks_strcmp(path, "/proc/self") == 0) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_syscall_procfs_is_list(const char *path) {
    return (path != CLKS_NULL && clks_strcmp(path, "/proc/list") == 0) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_syscall_parse_u64_dec(const char *text, u64 *out_value) {
    u64 value = 0ULL;
    usize i = 0U;

    if (text == CLKS_NULL || out_value == CLKS_NULL || text[0] == '\0') {
        return CLKS_FALSE;
    }

    while (text[i] != '\0') {
        u64 digit;

        if (text[i] < '0' || text[i] > '9') {
            return CLKS_FALSE;
        }

        digit = (u64)(text[i] - '0');

        if (value > ((0xFFFFFFFFFFFFFFFFULL - digit) / 10ULL)) {
            return CLKS_FALSE;
        }

        value = (value * 10ULL) + digit;
        i++;
    }

    *out_value = value;
    return CLKS_TRUE;
}

static clks_bool clks_syscall_procfs_parse_pid(const char *path, u64 *out_pid) {
    const char *part;
    usize i = 0U;
    char pid_text[32];
    u64 pid;

    if (path == CLKS_NULL || out_pid == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (path[0] != '/' || path[1] != 'p' || path[2] != 'r' || path[3] != 'o' || path[4] != 'c' || path[5] != '/') {
        return CLKS_FALSE;
    }

    part = &path[6];

    if (part[0] == '\0' || clks_strcmp(part, "self") == 0 || clks_strcmp(part, "list") == 0) {
        return CLKS_FALSE;
    }

    while (part[i] != '\0') {
        if (i + 1U >= sizeof(pid_text)) {
            return CLKS_FALSE;
        }

        if (part[i] < '0' || part[i] > '9') {
            return CLKS_FALSE;
        }

        pid_text[i] = part[i];
        i++;
    }

    pid_text[i] = '\0';

    if (clks_syscall_parse_u64_dec(pid_text, &pid) == CLKS_FALSE || pid == 0ULL) {
        return CLKS_FALSE;
    }

    *out_pid = pid;
    return CLKS_TRUE;
}

static const char *clks_syscall_proc_state_name(u64 state) {
    if (state == CLKS_EXEC_PROC_STATE_PENDING) {
        return "PENDING";
    }

    if (state == CLKS_EXEC_PROC_STATE_RUNNING) {
        return "RUNNING";
    }

    if (state == CLKS_EXEC_PROC_STATE_STOPPED) {
        return "STOPPED";
    }

    if (state == CLKS_EXEC_PROC_STATE_EXITED) {
        return "EXITED";
    }

    return "UNUSED";
}

static usize clks_syscall_procfs_append_char(char *out, usize out_size, usize pos, char ch) {
    if (out == CLKS_NULL || out_size == 0U) {
        return pos;
    }

    if (pos + 1U < out_size) {
        out[pos] = ch;
        out[pos + 1U] = '\0';
        return pos + 1U;
    }

    out[out_size - 1U] = '\0';
    return pos;
}

static usize clks_syscall_procfs_append_text(char *out, usize out_size, usize pos, const char *text) {
    usize i = 0U;

    if (text == CLKS_NULL) {
        return pos;
    }

    while (text[i] != '\0') {
        pos = clks_syscall_procfs_append_char(out, out_size, pos, text[i]);
        i++;
    }

    return pos;
}

static usize clks_syscall_procfs_append_u64_dec(char *out, usize out_size, usize pos, u64 value) {
    char temp[32];
    usize len = 0U;
    usize i;

    if (value == 0ULL) {
        return clks_syscall_procfs_append_char(out, out_size, pos, '0');
    }

    while (value != 0ULL && len + 1U < sizeof(temp)) {
        temp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    for (i = 0U; i < len; i++) {
        pos = clks_syscall_procfs_append_char(out, out_size, pos, temp[len - 1U - i]);
    }

    return pos;
}

static usize clks_syscall_procfs_append_u64_hex(char *out, usize out_size, usize pos, u64 value) {
    i32 nibble;

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "0X");

    for (nibble = 15; nibble >= 0; nibble--) {
        u64 current = (value >> (u64)(nibble * 4)) & 0x0FULL;
        char ch = (current < 10ULL) ? (char)('0' + current) : (char)('A' + (current - 10ULL));
        pos = clks_syscall_procfs_append_char(out, out_size, pos, ch);
    }

    return pos;
}

static clks_bool clks_syscall_procfs_snapshot_for_path(const char *path, struct clks_exec_proc_snapshot *out_snap) {
    u64 pid;

    if (path == CLKS_NULL || out_snap == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_syscall_procfs_is_self(path) == CLKS_TRUE) {
        pid = clks_exec_current_pid();

        if (pid == 0ULL) {
            return CLKS_FALSE;
        }

        return clks_exec_proc_snapshot(pid, out_snap);
    }

    if (clks_syscall_procfs_parse_pid(path, &pid) == CLKS_TRUE) {
        return clks_exec_proc_snapshot(pid, out_snap);
    }

    return CLKS_FALSE;
}

static usize clks_syscall_procfs_render_snapshot(char *out,
                                                 usize out_size,
                                                 const struct clks_exec_proc_snapshot *snap) {
    usize pos = 0U;

    if (out == CLKS_NULL || out_size == 0U || snap == CLKS_NULL) {
        return 0U;
    }

    out[0] = '\0';

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "pid=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->pid);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "ppid=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->ppid);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "state=");
    pos = clks_syscall_procfs_append_text(out, out_size, pos, clks_syscall_proc_state_name(snap->state));
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "state_id=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->state);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "tty=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->tty_index);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "runtime_ticks=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->runtime_ticks);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "mem_bytes=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->mem_bytes);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "exit_status=");
    pos = clks_syscall_procfs_append_u64_hex(out, out_size, pos, snap->exit_status);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "last_signal=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->last_signal);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "last_fault_vector=");
    pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap->last_fault_vector);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "last_fault_error=");
    pos = clks_syscall_procfs_append_u64_hex(out, out_size, pos, snap->last_fault_error);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "last_fault_rip=");
    pos = clks_syscall_procfs_append_u64_hex(out, out_size, pos, snap->last_fault_rip);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    pos = clks_syscall_procfs_append_text(out, out_size, pos, "path=");
    pos = clks_syscall_procfs_append_text(out, out_size, pos, snap->path);
    pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');

    return pos;
}

static usize clks_syscall_procfs_render_list(char *out, usize out_size) {
    usize pos = 0U;
    u64 proc_count = clks_exec_proc_count();
    u64 i;

    if (out == CLKS_NULL || out_size == 0U) {
        return 0U;
    }

    out[0] = '\0';
    pos = clks_syscall_procfs_append_text(out, out_size, pos, "pid state tty runtime mem path\n");

    for (i = 0ULL; i < proc_count; i++) {
        u64 pid = 0ULL;
        struct clks_exec_proc_snapshot snap;

        if (clks_exec_proc_pid_at(i, &pid) == CLKS_FALSE || pid == 0ULL) {
            continue;
        }

        if (clks_exec_proc_snapshot(pid, &snap) == CLKS_FALSE) {
            continue;
        }

        pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap.pid);
        pos = clks_syscall_procfs_append_char(out, out_size, pos, ' ');
        pos = clks_syscall_procfs_append_text(out, out_size, pos, clks_syscall_proc_state_name(snap.state));
        pos = clks_syscall_procfs_append_char(out, out_size, pos, ' ');
        pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap.tty_index);
        pos = clks_syscall_procfs_append_char(out, out_size, pos, ' ');
        pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap.runtime_ticks);
        pos = clks_syscall_procfs_append_char(out, out_size, pos, ' ');
        pos = clks_syscall_procfs_append_u64_dec(out, out_size, pos, snap.mem_bytes);
        pos = clks_syscall_procfs_append_char(out, out_size, pos, ' ');
        pos = clks_syscall_procfs_append_text(out, out_size, pos, snap.path);
        pos = clks_syscall_procfs_append_char(out, out_size, pos, '\n');
    }

    return pos;
}

static clks_bool clks_syscall_procfs_render_file(const char *path,
                                                 char *out,
                                                 usize out_size,
                                                 usize *out_len) {
    struct clks_exec_proc_snapshot snap;

    if (out_len != CLKS_NULL) {
        *out_len = 0U;
    }

    if (path == CLKS_NULL || out == CLKS_NULL || out_size == 0U || out_len == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_syscall_procfs_is_list(path) == CLKS_TRUE) {
        *out_len = clks_syscall_procfs_render_list(out, out_size);
        return CLKS_TRUE;
    }

    if (clks_syscall_procfs_snapshot_for_path(path, &snap) == CLKS_TRUE) {
        *out_len = clks_syscall_procfs_render_snapshot(out, out_size, &snap);
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

static u64 clks_syscall_fs_child_count(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_procfs_is_root(path) == CLKS_TRUE) {
        return 2ULL + clks_exec_proc_count();
    }

    return clks_fs_count_children(path);
}

static u64 clks_syscall_fs_get_child_name(u64 arg0, u64 arg1, u64 arg2) {
    char path[CLKS_SYSCALL_PATH_MAX];

    if (arg2 == 0ULL) {
        return 0ULL;
    }

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    if (clks_syscall_procfs_is_root(path) == CLKS_TRUE) {
        if (arg1 == 0ULL) {
            clks_memset((void *)arg2, 0, CLKS_SYSCALL_NAME_MAX);
            clks_memcpy((void *)arg2, "self", 5U);
            return 1ULL;
        }

        if (arg1 == 1ULL) {
            clks_memset((void *)arg2, 0, CLKS_SYSCALL_NAME_MAX);
            clks_memcpy((void *)arg2, "list", 5U);
            return 1ULL;
        }

        {
            u64 pid = 0ULL;
            char pid_text[32];
            usize len;

            if (clks_exec_proc_pid_at(arg1 - 2ULL, &pid) == CLKS_FALSE || pid == 0ULL) {
                return 0ULL;
            }

            clks_memset(pid_text, 0, sizeof(pid_text));
            len = clks_syscall_procfs_append_u64_dec(pid_text, sizeof(pid_text), 0U, pid);

            if (len + 1U > CLKS_SYSCALL_NAME_MAX) {
                return 0ULL;
            }

            clks_memset((void *)arg2, 0, CLKS_SYSCALL_NAME_MAX);
            clks_memcpy((void *)arg2, pid_text, len + 1U);
            return 1ULL;
        }
    }

    if (clks_fs_get_child_name(path, arg1, (char *)arg2, (usize)CLKS_SYSCALL_NAME_MAX) == CLKS_FALSE) {
        return 0ULL;
    }

    return 1ULL;
}

static u64 clks_syscall_fs_read(u64 arg0, u64 arg1, u64 arg2) {
    char path[CLKS_SYSCALL_PATH_MAX];
    const void *data;
    u64 file_size = 0ULL;
    u64 copy_len;

    if (arg1 == 0ULL || arg2 == 0ULL) {
        return 0ULL;
    }

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    if (clks_syscall_procfs_is_list(path) == CLKS_TRUE ||
        clks_syscall_procfs_is_self(path) == CLKS_TRUE ||
        clks_syscall_procfs_parse_pid(path, &file_size) == CLKS_TRUE) {
        char proc_text[CLKS_SYSCALL_PROCFS_TEXT_MAX];
        usize proc_len = 0U;

        if (clks_syscall_procfs_render_file(path, proc_text, sizeof(proc_text), &proc_len) == CLKS_FALSE) {
            return 0ULL;
        }

        copy_len = ((u64)proc_len < arg2) ? (u64)proc_len : arg2;

        if (copy_len == 0ULL) {
            return 0ULL;
        }

        clks_memcpy((void *)arg1, proc_text, (usize)copy_len);
        return copy_len;
    }

    data = clks_fs_read_all(path, &file_size);

    if (data == CLKS_NULL || file_size == 0ULL) {
        return 0ULL;
    }

    copy_len = (file_size < arg2) ? file_size : arg2;
    clks_memcpy((void *)arg1, data, (usize)copy_len);
    return copy_len;
}

static u64 clks_syscall_exec_path(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];
    u64 status = (u64)-1;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_exec_run_path(path, &status) == CLKS_FALSE) {
        return (u64)-1;
    }

    return status;
}

static u64 clks_syscall_exec_pathv(u64 arg0, u64 arg1, u64 arg2) {
    char path[CLKS_SYSCALL_PATH_MAX];
    char argv_line[CLKS_SYSCALL_ARG_LINE_MAX];
    char env_line[CLKS_SYSCALL_ENV_LINE_MAX];
    u64 status = (u64)-1;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_copy_user_optional_string(arg1, argv_line, sizeof(argv_line)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_copy_user_optional_string(arg2, env_line, sizeof(env_line)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_exec_run_pathv(path, argv_line, env_line, &status) == CLKS_FALSE) {
        return (u64)-1;
    }

    return status;
}

static u64 clks_syscall_getpid(void) {
    return clks_exec_current_pid();
}

static u64 clks_syscall_spawn_path(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];
    u64 pid = (u64)-1;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_exec_spawn_path(path, &pid) == CLKS_FALSE) {
        return (u64)-1;
    }

    return pid;
}

static u64 clks_syscall_spawn_pathv(u64 arg0, u64 arg1, u64 arg2) {
    char path[CLKS_SYSCALL_PATH_MAX];
    char argv_line[CLKS_SYSCALL_ARG_LINE_MAX];
    char env_line[CLKS_SYSCALL_ENV_LINE_MAX];
    u64 pid = (u64)-1;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_copy_user_optional_string(arg1, argv_line, sizeof(argv_line)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_copy_user_optional_string(arg2, env_line, sizeof(env_line)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_exec_spawn_pathv(path, argv_line, env_line, &pid) == CLKS_FALSE) {
        return (u64)-1;
    }

    return pid;
}

static u64 clks_syscall_waitpid(u64 arg0, u64 arg1) {
    u64 status = (u64)-1;
    u64 wait_ret = clks_exec_wait_pid(arg0, &status);

    if (wait_ret == 1ULL && arg1 != 0ULL) {
        clks_memcpy((void *)arg1, &status, sizeof(status));
    }

    return wait_ret;
}

static u64 clks_syscall_proc_argc(void) {
    return clks_exec_current_argc();
}

static u64 clks_syscall_proc_argv(u64 arg0, u64 arg1, u64 arg2) {
    if (arg1 == 0ULL || arg2 == 0ULL) {
        return 0ULL;
    }

    if (arg2 > CLKS_SYSCALL_ITEM_MAX) {
        arg2 = CLKS_SYSCALL_ITEM_MAX;
    }

    return (clks_exec_copy_current_argv(arg0, (char *)arg1, (usize)arg2) == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_proc_envc(void) {
    return clks_exec_current_envc();
}

static u64 clks_syscall_proc_env(u64 arg0, u64 arg1, u64 arg2) {
    if (arg1 == 0ULL || arg2 == 0ULL) {
        return 0ULL;
    }

    if (arg2 > CLKS_SYSCALL_ITEM_MAX) {
        arg2 = CLKS_SYSCALL_ITEM_MAX;
    }

    return (clks_exec_copy_current_env(arg0, (char *)arg1, (usize)arg2) == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_proc_last_signal(void) {
    return clks_exec_current_signal();
}

static u64 clks_syscall_proc_fault_vector(void) {
    return clks_exec_current_fault_vector();
}

static u64 clks_syscall_proc_fault_error(void) {
    return clks_exec_current_fault_error();
}

static u64 clks_syscall_proc_fault_rip(void) {
    return clks_exec_current_fault_rip();
}

static u64 clks_syscall_proc_count(void) {
    return clks_exec_proc_count();
}

static u64 clks_syscall_proc_pid_at(u64 arg0, u64 arg1) {
    u64 pid = 0ULL;

    if (arg1 == 0ULL) {
        return 0ULL;
    }

    if (clks_exec_proc_pid_at(arg0, &pid) == CLKS_FALSE) {
        return 0ULL;
    }

    clks_memcpy((void *)arg1, &pid, sizeof(pid));
    return 1ULL;
}

static u64 clks_syscall_proc_snapshot(u64 arg0, u64 arg1, u64 arg2) {
    struct clks_exec_proc_snapshot snap;

    if (arg1 == 0ULL || arg2 < (u64)sizeof(snap)) {
        return 0ULL;
    }

    if (clks_exec_proc_snapshot(arg0, &snap) == CLKS_FALSE) {
        return 0ULL;
    }

    clks_memcpy((void *)arg1, &snap, sizeof(snap));
    return 1ULL;
}

static u64 clks_syscall_proc_kill(u64 arg0, u64 arg1) {
    return clks_exec_proc_kill(arg0, arg1);
}

static u64 clks_syscall_exit(u64 arg0) {
    return (clks_exec_request_exit(arg0) == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_sleep_ticks(u64 arg0) {
    return clks_exec_sleep_ticks(arg0);
}

static u64 clks_syscall_yield(void) {
    return clks_exec_yield();
}

static u64 clks_syscall_shutdown(void) {
    clks_log(CLKS_LOG_WARN, "SYSCALL", "SHUTDOWN REQUESTED BY USERLAND");
    clks_serial_write("[WARN][SYSCALL] SHUTDOWN REQUESTED\n");
#if defined(CLKS_ARCH_X86_64)
    clks_syscall_outw(0x604U, 0x2000U);
#endif
    clks_cpu_halt_forever();
    return 1ULL;
}

static u64 clks_syscall_restart(void) {
    clks_log(CLKS_LOG_WARN, "SYSCALL", "RESTART REQUESTED BY USERLAND");
    clks_serial_write("[WARN][SYSCALL] RESTART REQUESTED\n");
#if defined(CLKS_ARCH_X86_64)
    clks_syscall_outb(0x64U, 0xFEU);
#endif
    clks_cpu_halt_forever();
    return 1ULL;
}


static u64 clks_syscall_audio_available(void) {
    return (clks_audio_available() == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_audio_play_tone(u64 arg0, u64 arg1) {
    if (clks_audio_play_tone(arg0, arg1) == CLKS_FALSE) {
        return 0ULL;
    }

    return 1ULL;
}

static u64 clks_syscall_audio_stop(void) {
    clks_audio_stop();
    return 1ULL;
}
static u64 clks_syscall_fs_stat_type(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];
    struct clks_fs_node_info info;
    struct clks_exec_proc_snapshot snap;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_procfs_is_root(path) == CLKS_TRUE) {
        return (u64)CLKS_FS_NODE_DIR;
    }

    if (clks_syscall_procfs_is_list(path) == CLKS_TRUE || clks_syscall_procfs_is_self(path) == CLKS_TRUE) {
        return (u64)CLKS_FS_NODE_FILE;
    }

    if (clks_syscall_procfs_snapshot_for_path(path, &snap) == CLKS_TRUE) {
        return (u64)CLKS_FS_NODE_FILE;
    }

    if (clks_fs_stat(path, &info) == CLKS_FALSE) {
        return (u64)-1;
    }

    return (u64)info.type;
}

static u64 clks_syscall_fs_stat_size(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];
    struct clks_fs_node_info info;
    char proc_text[CLKS_SYSCALL_PROCFS_TEXT_MAX];
    usize proc_len = 0U;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return (u64)-1;
    }

    if (clks_syscall_procfs_is_root(path) == CLKS_TRUE) {
        return 0ULL;
    }

    if (clks_syscall_procfs_render_file(path, proc_text, sizeof(proc_text), &proc_len) == CLKS_TRUE) {
        return (u64)proc_len;
    }

    if (clks_fs_stat(path, &info) == CLKS_FALSE) {
        return (u64)-1;
    }

    return info.size;
}

static u64 clks_syscall_fs_mkdir(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    return (clks_fs_mkdir(path) == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_fs_write_common(u64 arg0, u64 arg1, u64 arg2, clks_bool append_mode) {
    char path[CLKS_SYSCALL_PATH_MAX];
    void *heap_copy = CLKS_NULL;
    const void *payload = CLKS_NULL;
    clks_bool ok;

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    if (arg2 > CLKS_SYSCALL_FS_IO_MAX_LEN) {
        return 0ULL;
    }

    if (arg2 > 0ULL) {
        if (arg1 == 0ULL) {
            return 0ULL;
        }

        heap_copy = clks_kmalloc((usize)arg2);

        if (heap_copy == CLKS_NULL) {
            return 0ULL;
        }

        clks_memcpy(heap_copy, (const void *)arg1, (usize)arg2);
        payload = (const void *)heap_copy;
    }

    if (append_mode == CLKS_TRUE) {
        ok = clks_fs_append(path, payload, arg2);
    } else {
        ok = clks_fs_write_all(path, payload, arg2);
    }

    if (heap_copy != CLKS_NULL) {
        clks_kfree(heap_copy);
    }

    return (ok == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_fs_write(u64 arg0, u64 arg1, u64 arg2) {
    return clks_syscall_fs_write_common(arg0, arg1, arg2, CLKS_FALSE);
}

static u64 clks_syscall_fs_append(u64 arg0, u64 arg1, u64 arg2) {
    return clks_syscall_fs_write_common(arg0, arg1, arg2, CLKS_TRUE);
}

static u64 clks_syscall_fs_remove(u64 arg0) {
    char path[CLKS_SYSCALL_PATH_MAX];

    if (clks_syscall_copy_user_string(arg0, path, sizeof(path)) == CLKS_FALSE) {
        return 0ULL;
    }

    return (clks_fs_remove(path) == CLKS_TRUE) ? 1ULL : 0ULL;
}

static u64 clks_syscall_log_journal_count(void) {
    return clks_log_journal_count();
}

static u64 clks_syscall_log_journal_read(u64 arg0, u64 arg1, u64 arg2) {
    char line[CLKS_SYSCALL_JOURNAL_MAX_LEN];
    usize line_len;
    usize copy_len;

    if (arg1 == 0ULL || arg2 == 0ULL) {
        return 0ULL;
    }

    if (clks_log_journal_read(arg0, line, sizeof(line)) == CLKS_FALSE) {
        return 0ULL;
    }

    line_len = clks_strlen(line) + 1U;
    copy_len = line_len;

    if (copy_len > (usize)arg2) {
        copy_len = (usize)arg2;
    }

    if (copy_len > sizeof(line)) {
        copy_len = sizeof(line);
    }

    clks_memcpy((void *)arg1, line, copy_len);
    ((char *)arg1)[copy_len - 1U] = '\0';
    return 1ULL;
}

static void clks_syscall_serial_write_hex64(u64 value) {
    i32 nibble;

    for (nibble = 15; nibble >= 0; nibble--) {
        u64 current = (value >> (u64)(nibble * 4)) & 0x0FULL;
        char ch = (current < 10ULL) ? (char)('0' + current) : (char)('A' + (current - 10ULL));
        clks_serial_write_char(ch);
    }
}

static void clks_syscall_trace_user_program(u64 id) {
    clks_bool user_program_running =
        (clks_exec_is_running() == CLKS_TRUE && clks_exec_current_path_is_user() == CLKS_TRUE)
            ? CLKS_TRUE
            : CLKS_FALSE;

    if (user_program_running == CLKS_FALSE) {
        if (clks_syscall_user_trace_active == CLKS_TRUE) {
            clks_serial_write("[DEBUG][SYSCALL] USER_TRACE_END\n");
        }

        clks_syscall_user_trace_active = CLKS_FALSE;
        clks_syscall_user_trace_budget = 0ULL;
        return;
    }

    if (clks_syscall_user_trace_active == CLKS_FALSE) {
        clks_syscall_user_trace_active = CLKS_TRUE;
        clks_syscall_user_trace_budget = CLKS_SYSCALL_USER_TRACE_BUDGET;
        clks_serial_write("[DEBUG][SYSCALL] USER_TRACE_BEGIN\n");
        clks_serial_write("[DEBUG][SYSCALL] PID: 0X");
        clks_syscall_serial_write_hex64(clks_exec_current_pid());
        clks_serial_write("\n");
    }

    if (clks_syscall_user_trace_budget > 0ULL) {
        clks_serial_write("[DEBUG][SYSCALL] USER_ID: 0X");
        clks_syscall_serial_write_hex64(id);
        clks_serial_write("\n");
        clks_syscall_user_trace_budget--;

        if (clks_syscall_user_trace_budget == 0ULL) {
            clks_serial_write("[DEBUG][SYSCALL] USER_TRACE_BUDGET_EXHAUSTED\n");
        }
    }
}

void clks_syscall_init(void) {
    clks_syscall_ready = CLKS_TRUE;
    clks_syscall_user_trace_active = CLKS_FALSE;
    clks_syscall_user_trace_budget = 0ULL;
    clks_log(CLKS_LOG_INFO, "SYSCALL", "INT80 FRAMEWORK ONLINE");
}

u64 clks_syscall_dispatch(void *frame_ptr) {
    struct clks_syscall_frame *frame = (struct clks_syscall_frame *)frame_ptr;
    u64 id;

    if (clks_syscall_ready == CLKS_FALSE || frame == CLKS_NULL) {
        return (u64)-1;
    }

    id = frame->rax;
    clks_syscall_trace_user_program(id);

    switch (id) {
        case CLKS_SYSCALL_LOG_WRITE:
            return clks_syscall_log_write(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_TIMER_TICKS:
            return clks_interrupts_timer_ticks();
        case CLKS_SYSCALL_TASK_COUNT: {
            struct clks_scheduler_stats stats = clks_scheduler_get_stats();
            return stats.task_count;
        }
        case CLKS_SYSCALL_CURRENT_TASK_ID: {
            struct clks_scheduler_stats stats = clks_scheduler_get_stats();
            return stats.current_task_id;
        }
        case CLKS_SYSCALL_SERVICE_COUNT:
            return clks_service_count();
        case CLKS_SYSCALL_SERVICE_READY_COUNT:
            return clks_service_ready_count();
        case CLKS_SYSCALL_CONTEXT_SWITCHES: {
            struct clks_scheduler_stats stats = clks_scheduler_get_stats();
            return stats.context_switch_count;
        }
        case CLKS_SYSCALL_KELF_COUNT:
            return clks_kelf_count();
        case CLKS_SYSCALL_KELF_RUNS:
            return clks_kelf_total_runs();
        case CLKS_SYSCALL_FS_NODE_COUNT:
            return clks_fs_node_count();
        case CLKS_SYSCALL_FS_CHILD_COUNT:
            return clks_syscall_fs_child_count(frame->rbx);
        case CLKS_SYSCALL_FS_GET_CHILD_NAME:
            return clks_syscall_fs_get_child_name(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_FS_READ:
            return clks_syscall_fs_read(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_EXEC_PATH:
            return clks_syscall_exec_path(frame->rbx);
        case CLKS_SYSCALL_EXEC_PATHV:
            return clks_syscall_exec_pathv(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_EXEC_REQUESTS:
            return clks_exec_request_count();
        case CLKS_SYSCALL_EXEC_SUCCESS:
            return clks_exec_success_count();
        case CLKS_SYSCALL_USER_SHELL_READY:
            return (clks_userland_shell_ready() == CLKS_TRUE) ? 1ULL : 0ULL;
        case CLKS_SYSCALL_USER_EXEC_REQUESTED:
            return (clks_userland_shell_exec_requested() == CLKS_TRUE) ? 1ULL : 0ULL;
        case CLKS_SYSCALL_USER_LAUNCH_TRIES:
            return clks_userland_launch_attempts();
        case CLKS_SYSCALL_USER_LAUNCH_OK:
            return clks_userland_launch_success();
        case CLKS_SYSCALL_USER_LAUNCH_FAIL:
            return clks_userland_launch_failures();
        case CLKS_SYSCALL_TTY_COUNT:
            return (u64)clks_tty_count();
        case CLKS_SYSCALL_TTY_ACTIVE:
            return (u64)clks_tty_active();
        case CLKS_SYSCALL_TTY_SWITCH:
            clks_tty_switch((u32)frame->rbx);
            return (u64)clks_tty_active();
        case CLKS_SYSCALL_TTY_WRITE:
            return clks_syscall_tty_write(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_TTY_WRITE_CHAR:
            return clks_syscall_tty_write_char(frame->rbx);
        case CLKS_SYSCALL_KBD_GET_CHAR:
            return clks_syscall_kbd_get_char();
        case CLKS_SYSCALL_FS_STAT_TYPE:
            return clks_syscall_fs_stat_type(frame->rbx);
        case CLKS_SYSCALL_FS_STAT_SIZE:
            return clks_syscall_fs_stat_size(frame->rbx);
        case CLKS_SYSCALL_FS_MKDIR:
            return clks_syscall_fs_mkdir(frame->rbx);
        case CLKS_SYSCALL_FS_WRITE:
            return clks_syscall_fs_write(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_FS_APPEND:
            return clks_syscall_fs_append(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_FS_REMOVE:
            return clks_syscall_fs_remove(frame->rbx);
        case CLKS_SYSCALL_LOG_JOURNAL_COUNT:
            return clks_syscall_log_journal_count();
        case CLKS_SYSCALL_LOG_JOURNAL_READ:
            return clks_syscall_log_journal_read(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_KBD_BUFFERED:
            return clks_keyboard_buffered_count();
        case CLKS_SYSCALL_KBD_PUSHED:
            return clks_keyboard_push_count();
        case CLKS_SYSCALL_KBD_POPPED:
            return clks_keyboard_pop_count();
        case CLKS_SYSCALL_KBD_DROPPED:
            return clks_keyboard_drop_count();
        case CLKS_SYSCALL_KBD_HOTKEY_SWITCHES:
            return clks_keyboard_hotkey_switch_count();
        case CLKS_SYSCALL_GETPID:
            return clks_syscall_getpid();
        case CLKS_SYSCALL_SPAWN_PATH:
            return clks_syscall_spawn_path(frame->rbx);
        case CLKS_SYSCALL_SPAWN_PATHV:
            return clks_syscall_spawn_pathv(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_WAITPID:
            return clks_syscall_waitpid(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_PROC_ARGC:
            return clks_syscall_proc_argc();
        case CLKS_SYSCALL_PROC_ARGV:
            return clks_syscall_proc_argv(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_PROC_ENVC:
            return clks_syscall_proc_envc();
        case CLKS_SYSCALL_PROC_ENV:
            return clks_syscall_proc_env(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_PROC_LAST_SIGNAL:
            return clks_syscall_proc_last_signal();
        case CLKS_SYSCALL_PROC_FAULT_VECTOR:
            return clks_syscall_proc_fault_vector();
        case CLKS_SYSCALL_PROC_FAULT_ERROR:
            return clks_syscall_proc_fault_error();
        case CLKS_SYSCALL_PROC_FAULT_RIP:
            return clks_syscall_proc_fault_rip();
        case CLKS_SYSCALL_PROC_COUNT:
            return clks_syscall_proc_count();
        case CLKS_SYSCALL_PROC_PID_AT:
            return clks_syscall_proc_pid_at(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_PROC_SNAPSHOT:
            return clks_syscall_proc_snapshot(frame->rbx, frame->rcx, frame->rdx);
        case CLKS_SYSCALL_PROC_KILL:
            return clks_syscall_proc_kill(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_EXIT:
            return clks_syscall_exit(frame->rbx);
        case CLKS_SYSCALL_SLEEP_TICKS:
            return clks_syscall_sleep_ticks(frame->rbx);
        case CLKS_SYSCALL_YIELD:
            return clks_syscall_yield();
        case CLKS_SYSCALL_SHUTDOWN:
            return clks_syscall_shutdown();
        case CLKS_SYSCALL_RESTART:
            return clks_syscall_restart();
        case CLKS_SYSCALL_AUDIO_AVAILABLE:
            return clks_syscall_audio_available();
        case CLKS_SYSCALL_AUDIO_PLAY_TONE:
            return clks_syscall_audio_play_tone(frame->rbx, frame->rcx);
        case CLKS_SYSCALL_AUDIO_STOP:
            return clks_syscall_audio_stop();
        default:
            return (u64)-1;
    }
}

u64 clks_syscall_invoke_kernel(u64 id, u64 arg0, u64 arg1, u64 arg2) {
    u64 ret;

    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(id), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory"
    );

    return ret;
}
