#include <clks/cpu.h>
#include <clks/framebuffer.h>
#include <clks/fs.h>
#include <clks/panic.h>
#include <clks/serial.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_PANIC_BG 0x00200000U
#define CLKS_PANIC_FG 0x00FFE0E0U

#define CLKS_PANIC_BACKTRACE_MAX        20U
#define CLKS_PANIC_STACK_WINDOW_BYTES   (128ULL * 1024ULL)
#define CLKS_PANIC_SYMBOL_FILE          "/system/kernel.sym"
#define CLKS_PANIC_KERNEL_ADDR_BASE     0xFFFF800000000000ULL

struct clks_panic_console {
    u32 cols;
    u32 rows;
    u32 row;
    u32 col;
    u32 cell_w;
    u32 cell_h;
};

static clks_bool clks_panic_active = CLKS_FALSE;
static clks_bool clks_panic_symbols_checked = CLKS_FALSE;
static const char *clks_panic_symbols_data = CLKS_NULL;
static u64 clks_panic_symbols_size = 0ULL;

static inline void clks_panic_disable_interrupts(void) {
#if defined(CLKS_ARCH_X86_64)
    __asm__ volatile("cli");
#elif defined(CLKS_ARCH_AARCH64)
    __asm__ volatile("msr daifset, #0xf");
#endif
}

static void clks_panic_u64_to_hex(u64 value, char out[19]) {
    int nibble;

    out[0] = '0';
    out[1] = 'X';

    for (nibble = 0; nibble < 16; nibble++) {
        u8 current = (u8)((value >> ((15 - nibble) * 4)) & 0x0FULL);
        out[2 + nibble] = (current < 10U) ? (char)('0' + current) : (char)('A' + (current - 10U));
    }

    out[18] = '\0';
}

static void clks_panic_u32_to_dec(u32 value, char *out, usize out_size) {
    char tmp[11];
    usize len = 0U;
    usize i;

    if (out == CLKS_NULL || out_size == 0U) {
        return;
    }

    if (value == 0U) {
        if (out_size >= 2U) {
            out[0] = '0';
            out[1] = '\0';
        } else {
            out[0] = '\0';
        }
        return;
    }

    while (value != 0U && len < sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    if (len + 1U > out_size) {
        len = out_size - 1U;
    }

    for (i = 0U; i < len; i++) {
        out[i] = tmp[len - 1U - i];
    }

    out[len] = '\0';
}

static clks_bool clks_panic_console_init(struct clks_panic_console *console) {
    struct clks_framebuffer_info info;

    if (console == CLKS_NULL || clks_fb_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    info = clks_fb_info();

    console->cell_w = clks_fb_cell_width();
    console->cell_h = clks_fb_cell_height();

    if (console->cell_w == 0U) {
        console->cell_w = 8U;
    }

    if (console->cell_h == 0U) {
        console->cell_h = 8U;
    }

    console->cols = info.width / console->cell_w;
    console->rows = info.height / console->cell_h;
    console->row = 0U;
    console->col = 0U;

    if (console->cols == 0U || console->rows == 0U) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static void clks_panic_console_newline(struct clks_panic_console *console) {
    if (console == CLKS_NULL) {
        return;
    }

    console->col = 0U;

    if (console->row + 1U < console->rows) {
        console->row++;
    }
}

static void clks_panic_console_put_char(struct clks_panic_console *console, char ch) {
    u32 x;
    u32 y;

    if (console == CLKS_NULL) {
        return;
    }

    if (ch == '\n') {
        clks_panic_console_newline(console);
        return;
    }

    if (ch == '\r') {
        console->col = 0U;
        return;
    }

    if (console->row >= console->rows || console->col >= console->cols) {
        return;
    }

    x = console->col * console->cell_w;
    y = console->row * console->cell_h;
    clks_fb_draw_char(x, y, ch, CLKS_PANIC_FG, CLKS_PANIC_BG);

    console->col++;

    if (console->col >= console->cols) {
        clks_panic_console_newline(console);
    }
}

static void clks_panic_console_write_n(struct clks_panic_console *console, const char *text, usize len) {
    usize i = 0U;

    if (console == CLKS_NULL || text == CLKS_NULL) {
        return;
    }

    while (i < len) {
        clks_panic_console_put_char(console, text[i]);
        i++;
    }
}

static void clks_panic_console_write(struct clks_panic_console *console, const char *text) {
    if (console == CLKS_NULL || text == CLKS_NULL) {
        return;
    }

    clks_panic_console_write_n(console, text, clks_strlen(text));
}

static void clks_panic_serial_write_n(const char *text, usize len) {
    usize i = 0U;

    if (text == CLKS_NULL) {
        return;
    }

    while (i < len) {
        clks_serial_write_char(text[i]);
        i++;
    }
}

static void clks_panic_serial_write_line(const char *line) {
    if (line == CLKS_NULL) {
        return;
    }

    clks_serial_write(line);
    clks_serial_write("\n");
}

static clks_bool clks_panic_is_hex(char ch) {
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

static u8 clks_panic_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return (u8)(ch - '0');
    }

    if (ch >= 'a' && ch <= 'f') {
        return (u8)(10 + (ch - 'a'));
    }

    return (u8)(10 + (ch - 'A'));
}

static clks_bool clks_panic_parse_symbol_line(const char *line,
                                              usize len,
                                              u64 *out_addr,
                                              const char **out_name,
                                              usize *out_name_len,
                                              const char **out_source,
                                              usize *out_source_len) {
    usize i = 0U;
    u64 addr = 0ULL;
    u32 digits = 0U;
    usize name_start;
    usize name_end;
    usize source_start;
    usize source_end;

    if (line == CLKS_NULL || out_addr == CLKS_NULL || out_name == CLKS_NULL || out_name_len == CLKS_NULL ||
        out_source == CLKS_NULL || out_source_len == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (len == 0U) {
        return CLKS_FALSE;
    }

    if (len >= 2U && line[0] == '0' && (line[1] == 'X' || line[1] == 'x')) {
        i = 2U;
    }

    while (i < len && clks_panic_is_hex(line[i]) == CLKS_TRUE) {
        addr = (addr << 4) | (u64)clks_panic_hex_value(line[i]);
        digits++;
        i++;
    }

    if (digits == 0U) {
        return CLKS_FALSE;
    }

    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }

    if (i >= len) {
        return CLKS_FALSE;
    }

    name_start = i;

    while (i < len && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') {
        i++;
    }

    name_end = i;

    if (name_end <= name_start) {
        return CLKS_FALSE;
    }

    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }

    source_start = i;
    source_end = len;

    while (source_end > source_start &&
           (line[source_end - 1U] == ' ' || line[source_end - 1U] == '\t' || line[source_end - 1U] == '\r')) {
        source_end--;
    }

    *out_addr = addr;
    *out_name = &line[name_start];
    *out_name_len = name_end - name_start;
    *out_source = (source_end > source_start) ? &line[source_start] : CLKS_NULL;
    *out_source_len = (source_end > source_start) ? (source_end - source_start) : 0U;
    return CLKS_TRUE;
}

static clks_bool clks_panic_symbols_ready(void) {
    u64 size = 0ULL;
    const void *data;

    if (clks_panic_symbols_checked == CLKS_TRUE) {
        return (clks_panic_symbols_data != CLKS_NULL && clks_panic_symbols_size > 0ULL) ? CLKS_TRUE : CLKS_FALSE;
    }

    clks_panic_symbols_checked = CLKS_TRUE;

    if (clks_fs_is_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    data = clks_fs_read_all(CLKS_PANIC_SYMBOL_FILE, &size);

    if (data == CLKS_NULL || size == 0ULL) {
        return CLKS_FALSE;
    }

    clks_panic_symbols_data = (const char *)data;
    clks_panic_symbols_size = size;
    return CLKS_TRUE;
}

static clks_bool clks_panic_lookup_symbol(u64 addr,
                                          const char **out_name,
                                          usize *out_name_len,
                                          u64 *out_base,
                                          const char **out_source,
                                          usize *out_source_len) {
    const char *data;
    const char *end;
    const char *line;
    const char *best_name = CLKS_NULL;
    const char *best_source = CLKS_NULL;
    usize best_len = 0U;
    usize best_source_len = 0U;
    u64 best_addr = 0ULL;
    clks_bool found = CLKS_FALSE;

    if (out_name == CLKS_NULL || out_name_len == CLKS_NULL || out_base == CLKS_NULL || out_source == CLKS_NULL ||
        out_source_len == CLKS_NULL) {
        return CLKS_FALSE;
    }

    *out_name = CLKS_NULL;
    *out_name_len = 0U;
    *out_base = 0ULL;
    *out_source = CLKS_NULL;
    *out_source_len = 0U;

    if (clks_panic_symbols_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    data = clks_panic_symbols_data;
    end = clks_panic_symbols_data + clks_panic_symbols_size;

    while (data < end) {
        u64 line_addr;
        const char *line_name;
        usize line_name_len;
        const char *line_source;
        usize line_source_len;
        usize line_len = 0U;

        line = data;

        while (data < end && *data != '\n') {
            data++;
            line_len++;
        }

        if (data < end && *data == '\n') {
            data++;
        }

        if (clks_panic_parse_symbol_line(line,
                                         line_len,
                                         &line_addr,
                                         &line_name,
                                         &line_name_len,
                                         &line_source,
                                         &line_source_len) == CLKS_FALSE) {
            continue;
        }

        if (line_addr <= addr && (found == CLKS_FALSE || line_addr >= best_addr)) {
            best_addr = line_addr;
            best_name = line_name;
            best_len = line_name_len;
            best_source = line_source;
            best_source_len = line_source_len;
            found = CLKS_TRUE;
        }
    }

    if (found == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    *out_name = best_name;
    *out_name_len = best_len;
    *out_base = best_addr;
    *out_source = best_source;
    *out_source_len = best_source_len;
    return CLKS_TRUE;
}

static void clks_panic_emit_bt_entry(struct clks_panic_console *console, u32 index, u64 rip) {
    char index_dec[12];
    char rip_hex[19];
    const char *sym_name = CLKS_NULL;
    const char *sym_source = CLKS_NULL;
    usize sym_name_len = 0U;
    usize sym_source_len = 0U;
    u64 sym_base = 0ULL;
    clks_bool has_symbol;

    clks_panic_u32_to_dec(index, index_dec, sizeof(index_dec));
    clks_panic_u64_to_hex(rip, rip_hex);
    has_symbol = clks_panic_lookup_symbol(rip, &sym_name, &sym_name_len, &sym_base, &sym_source, &sym_source_len);

    clks_serial_write("[PANIC][BT] #");
    clks_serial_write(index_dec);
    clks_serial_write(" ");
    clks_serial_write(rip_hex);

    if (has_symbol == CLKS_TRUE) {
        char off_hex[19];
        u64 off = rip - sym_base;

        clks_panic_u64_to_hex(off, off_hex);
        clks_serial_write(" ");
        clks_panic_serial_write_n(sym_name, sym_name_len);
        clks_serial_write("+");
        clks_serial_write(off_hex);

        if (sym_source != CLKS_NULL && sym_source_len > 0U) {
            clks_serial_write(" @ ");
            clks_panic_serial_write_n(sym_source, sym_source_len);
        }
    }

    clks_serial_write("\n");

    if (console == CLKS_NULL) {
        return;
    }

    clks_panic_console_write(console, "BT#");
    clks_panic_console_write(console, index_dec);
    clks_panic_console_write(console, " ");
    clks_panic_console_write(console, rip_hex);

    if (has_symbol == CLKS_TRUE) {
        char off_hex[19];
        u64 off = rip - sym_base;

        clks_panic_u64_to_hex(off, off_hex);
        clks_panic_console_write(console, " ");
        clks_panic_console_write_n(console, sym_name, sym_name_len);
        clks_panic_console_write(console, "+");
        clks_panic_console_write(console, off_hex);

        if (sym_source != CLKS_NULL && sym_source_len > 0U) {
            clks_panic_console_write(console, " @ ");
            clks_panic_console_write_n(console, sym_source, sym_source_len);
        }
    }

    clks_panic_console_write(console, "\n");
}

static clks_bool clks_panic_stack_ptr_valid(u64 ptr, u64 stack_low, u64 stack_high) {
    if ((ptr & 0x7ULL) != 0ULL) {
        return CLKS_FALSE;
    }

    if (ptr < stack_low || ptr + 16ULL > stack_high) {
        return CLKS_FALSE;
    }

    if (ptr < CLKS_PANIC_KERNEL_ADDR_BASE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static void clks_panic_emit_backtrace(struct clks_panic_console *console, u64 rip, u64 rbp, u64 rsp) {
    u64 current_rbp;
    u64 stack_low;
    u64 stack_high;
    u32 frame = 0U;

    if (rip == 0ULL) {
        return;
    }

    clks_panic_serial_write_line("[PANIC][BT] BEGIN");

    if (console != CLKS_NULL) {
        clks_panic_console_write(console, "\nBACKTRACE:\n");
    }

    clks_panic_emit_bt_entry(console, frame, rip);
    frame++;

    if (rbp == 0ULL || rsp == 0ULL || frame >= CLKS_PANIC_BACKTRACE_MAX) {
        clks_panic_serial_write_line("[PANIC][BT] END");
        return;
    }

    stack_low = rsp;
    stack_high = rsp + CLKS_PANIC_STACK_WINDOW_BYTES;

    if (stack_high <= stack_low) {
        clks_panic_serial_write_line("[PANIC][BT] END");
        return;
    }

    current_rbp = rbp;

    while (frame < CLKS_PANIC_BACKTRACE_MAX) {
        const u64 *frame_ptr;
        u64 next_rbp;
        u64 ret_rip;

        if (clks_panic_stack_ptr_valid(current_rbp, stack_low, stack_high) == CLKS_FALSE) {
            break;
        }

        frame_ptr = (const u64 *)(usize)current_rbp;
        next_rbp = frame_ptr[0];
        ret_rip = frame_ptr[1];

        if (ret_rip == 0ULL) {
            break;
        }

        clks_panic_emit_bt_entry(console, frame, ret_rip);
        frame++;

        if (next_rbp <= current_rbp) {
            break;
        }

        current_rbp = next_rbp;
    }

    clks_panic_serial_write_line("[PANIC][BT] END");
}

static void clks_panic_capture_context(u64 *out_rip, u64 *out_rbp, u64 *out_rsp) {
    if (out_rip != CLKS_NULL) {
        *out_rip = 0ULL;
    }

    if (out_rbp != CLKS_NULL) {
        *out_rbp = 0ULL;
    }

    if (out_rsp != CLKS_NULL) {
        *out_rsp = 0ULL;
    }

#if defined(CLKS_ARCH_X86_64)
    if (out_rbp != CLKS_NULL) {
        __asm__ volatile("mov %%rbp, %0" : "=r"(*out_rbp));
    }

    if (out_rsp != CLKS_NULL) {
        __asm__ volatile("mov %%rsp, %0" : "=r"(*out_rsp));
    }

    if (out_rip != CLKS_NULL) {
        *out_rip = (u64)(usize)__builtin_return_address(0);
    }
#endif
}

static CLKS_NORETURN void clks_panic_halt_loop(void) {
    clks_cpu_halt_forever();
}

CLKS_NORETURN void clks_panic(const char *reason) {
    struct clks_panic_console console;
    u64 rip = 0ULL;
    u64 rbp = 0ULL;
    u64 rsp = 0ULL;

    clks_panic_disable_interrupts();

    if (clks_panic_active == CLKS_TRUE) {
        clks_panic_halt_loop();
    }

    clks_panic_active = CLKS_TRUE;
    clks_panic_capture_context(&rip, &rbp, &rsp);

    clks_panic_serial_write_line("[PANIC] CLeonOS KERNEL PANIC");

    if (reason != CLKS_NULL) {
        clks_panic_serial_write_line(reason);
    }

    if (clks_panic_console_init(&console) == CLKS_TRUE) {
        clks_fb_clear(CLKS_PANIC_BG);

        clks_panic_console_write(&console, "CLeonOS KERNEL PANIC\n");
        clks_panic_console_write(&console, "====================\n\n");

        if (reason != CLKS_NULL) {
            clks_panic_console_write(&console, "REASON: ");
            clks_panic_console_write(&console, reason);
            clks_panic_console_write(&console, "\n");
        }

        clks_panic_emit_backtrace(&console, rip, rbp, rsp);
        clks_panic_console_write(&console, "\nSystem halted. Please reboot the VM.\n");
    } else {
        clks_panic_emit_backtrace(CLKS_NULL, rip, rbp, rsp);
    }

    clks_panic_halt_loop();
}

CLKS_NORETURN void clks_panic_exception(const char *name,
                                        u64 vector,
                                        u64 error_code,
                                        u64 rip,
                                        u64 rbp,
                                        u64 rsp) {
    struct clks_panic_console console;
    char hex_buf[19];

    clks_panic_disable_interrupts();

    if (clks_panic_active == CLKS_TRUE) {
        clks_panic_halt_loop();
    }

    clks_panic_active = CLKS_TRUE;

    clks_panic_serial_write_line("[PANIC] CPU EXCEPTION");

    if (name != CLKS_NULL) {
        clks_panic_serial_write_line(name);
    }

    clks_panic_u64_to_hex(vector, hex_buf);
    clks_panic_serial_write_line(hex_buf);
    clks_panic_u64_to_hex(error_code, hex_buf);
    clks_panic_serial_write_line(hex_buf);
    clks_panic_u64_to_hex(rip, hex_buf);
    clks_panic_serial_write_line(hex_buf);

    if (clks_panic_console_init(&console) == CLKS_TRUE) {
        clks_fb_clear(CLKS_PANIC_BG);

        clks_panic_console_write(&console, "CLeonOS KERNEL PANIC\n");
        clks_panic_console_write(&console, "====================\n\n");
        clks_panic_console_write(&console, "TYPE: CPU EXCEPTION\n");

        if (name != CLKS_NULL) {
            clks_panic_console_write(&console, "NAME: ");
            clks_panic_console_write(&console, name);
            clks_panic_console_write(&console, "\n");
        }

        clks_panic_u64_to_hex(vector, hex_buf);
        clks_panic_console_write(&console, "VECTOR: ");
        clks_panic_console_write(&console, hex_buf);
        clks_panic_console_write(&console, "\n");

        clks_panic_u64_to_hex(error_code, hex_buf);
        clks_panic_console_write(&console, "ERROR:  ");
        clks_panic_console_write(&console, hex_buf);
        clks_panic_console_write(&console, "\n");

        clks_panic_u64_to_hex(rip, hex_buf);
        clks_panic_console_write(&console, "RIP:    ");
        clks_panic_console_write(&console, hex_buf);
        clks_panic_console_write(&console, "\n");

        clks_panic_emit_backtrace(&console, rip, rbp, rsp);
        clks_panic_console_write(&console, "\nSystem halted. Please reboot the VM.\n");
    } else {
        clks_panic_emit_backtrace(CLKS_NULL, rip, rbp, rsp);
    }

    clks_panic_halt_loop();
}
