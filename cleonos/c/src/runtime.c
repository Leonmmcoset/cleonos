#include <cleonos_syscall.h>

typedef int (*cleonos_entry_fn)(void);

extern int cleonos_app_main(void);

void _start(void) {
    volatile int code;

    code = ((cleonos_entry_fn)cleonos_app_main)();
    (void)code;

    for (;;) {
        __asm__ volatile("pause");
    }
}
