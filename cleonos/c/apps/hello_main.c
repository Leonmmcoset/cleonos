#include <cleonos_syscall.h>

int cleonos_app_main(void) {
    static const char msg[] = "[USER][HELLO] Hello world from /hello.elf\n";
    (void)cleonos_sys_tty_write(msg, (u64)(sizeof(msg) - 1U));
    return 0;
}
