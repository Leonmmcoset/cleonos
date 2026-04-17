#include <cleonos_syscall.h>

int cleonos_app_main(void) {
    static const char banner[] =
        "spin: busy loop started (test Alt+Ctrl+C force stop)\n";
    volatile u64 noise = 0xC1E0C1E0ULL;

    (void)cleonos_sys_tty_write(banner, (u64)(sizeof(banner) - 1U));

    for (;;) {
        noise ^= (noise << 7);
        noise ^= (noise >> 9);
        noise ^= 0x9E3779B97F4A7C15ULL;
        noise += 0xA5A5A5A5A5A5A5A5ULL;
    }
}
