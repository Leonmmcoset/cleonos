#include <cleonos_rust_bridge.h>
#include <cleonos_syscall.h>

static const char shell_banner[] = "[USER][SHELL] shell.elf online";
static const char shell_status[] = "[USER][SHELL] syscall int80 path ok";

int cleonos_app_main(void) {
    u64 len = cleonos_rust_guarded_len((const unsigned char *)shell_banner, (usize)(sizeof(shell_banner) - 1U));

    cleonos_sys_log_write(shell_banner, len);
    cleonos_sys_log_write(shell_status, (u64)(sizeof(shell_status) - 1U));
    return 0;
}
