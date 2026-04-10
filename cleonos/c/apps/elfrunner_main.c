#include <cleonos_syscall.h>

static const char elfrunner_banner[] = "[KAPP][ELFRUNNER] elfrunner.elf online";

int cleonos_app_main(void) {
    cleonos_sys_log_write(elfrunner_banner, (u64)(sizeof(elfrunner_banner) - 1U));
    return 0;
}
