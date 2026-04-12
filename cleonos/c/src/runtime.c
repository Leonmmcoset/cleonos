#include <cleonos_syscall.h>

typedef int (*cleonos_entry_fn)(void);

extern int cleonos_app_main(void);

u64 _start(void) {
    int code;

    code = ((cleonos_entry_fn)cleonos_app_main)();
    return (u64)code;
}
