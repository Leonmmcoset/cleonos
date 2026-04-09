#include <clks/compiler.h>
#include <clks/serial.h>
#include <clks/types.h>

#if defined(CLKS_ARCH_X86_64)

#define CLKS_COM1_PORT 0x3F8

static inline void clks_x86_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 clks_x86_inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void clks_serial_init(void) {
    clks_x86_outb(CLKS_COM1_PORT + 1, 0x00);
    clks_x86_outb(CLKS_COM1_PORT + 3, 0x80);
    clks_x86_outb(CLKS_COM1_PORT + 0, 0x03);
    clks_x86_outb(CLKS_COM1_PORT + 1, 0x00);
    clks_x86_outb(CLKS_COM1_PORT + 3, 0x03);
    clks_x86_outb(CLKS_COM1_PORT + 2, 0xC7);
    clks_x86_outb(CLKS_COM1_PORT + 4, 0x0B);
}

void clks_serial_write_char(char ch) {
    while ((clks_x86_inb(CLKS_COM1_PORT + 5) & 0x20) == 0) {
    }

    clks_x86_outb(CLKS_COM1_PORT, (u8)ch);
}

#elif defined(CLKS_ARCH_AARCH64)

#define CLKS_PL011_BASE 0x09000000ULL
#define CLKS_PL011_DR   (*(volatile u32 *)(CLKS_PL011_BASE + 0x00))
#define CLKS_PL011_FR   (*(volatile u32 *)(CLKS_PL011_BASE + 0x18))
#define CLKS_PL011_TXFF (1U << 5)

void clks_serial_init(void) {
}

void clks_serial_write_char(char ch) {
    while ((CLKS_PL011_FR & CLKS_PL011_TXFF) != 0) {
    }

    CLKS_PL011_DR = (u32)(u8)ch;
}

#else
#error "Unsupported architecture"
#endif

void clks_serial_write(const char *text) {
    usize i = 0;

    while (text[i] != '\0') {
        if (text[i] == '\n') {
            clks_serial_write_char('\r');
        }

        clks_serial_write_char(text[i]);
        i++;
    }
}