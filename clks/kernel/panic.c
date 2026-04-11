#include <clks/cpu.h>
#include <clks/framebuffer.h>
#include <clks/panic.h>
#include <clks/serial.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_PANIC_BG 0x00200000U
#define CLKS_PANIC_FG 0x00FFE0E0U

struct clks_panic_console {
    u32 cols;
    u32 rows;
    u32 row;
    u32 col;
    u32 cell_w;
    u32 cell_h;
};

static clks_bool clks_panic_active = CLKS_FALSE;

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

static void clks_panic_console_write(struct clks_panic_console *console, const char *text) {
    usize i = 0U;

    if (console == CLKS_NULL || text == CLKS_NULL) {
        return;
    }

    while (text[i] != '\0') {
        clks_panic_console_put_char(console, text[i]);
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

static CLKS_NORETURN void clks_panic_halt_loop(void) {
    clks_cpu_halt_forever();
}

CLKS_NORETURN void clks_panic(const char *reason) {
    struct clks_panic_console console;

    clks_panic_disable_interrupts();

    if (clks_panic_active == CLKS_TRUE) {
        clks_panic_halt_loop();
    }

    clks_panic_active = CLKS_TRUE;

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

        clks_panic_console_write(&console, "\nSystem halted. Please reboot the VM.\n");
    }

    clks_panic_halt_loop();
}

CLKS_NORETURN void clks_panic_exception(const char *name, u64 vector, u64 error_code, u64 rip) {
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
        clks_panic_console_write(&console, "\n\n");

        clks_panic_console_write(&console, "System halted. Please reboot the VM.\n");
    }

    clks_panic_halt_loop();
}