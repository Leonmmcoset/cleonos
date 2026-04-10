#include <cleonos_syscall.h>

static const char memc_banner[] = "[KAPP][MEMC] memc.elf online";

int cleonos_app_main(void) {
    u64 ticks = cleonos_sys_timer_ticks();
    (void)ticks;
    cleonos_sys_log_write(memc_banner, (u64)(sizeof(memc_banner) - 1U));
    return 0;
}
