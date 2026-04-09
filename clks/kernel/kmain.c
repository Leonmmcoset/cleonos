#include <clks/boot.h>
#include <clks/cpu.h>
#include <clks/framebuffer.h>
#include <clks/kernel.h>
#include <clks/log.h>
#include <clks/serial.h>
#include <clks/tty.h>
#include <clks/types.h>

void clks_kernel_main(void) {
    const struct limine_framebuffer *boot_fb;

    clks_serial_init();

    if (clks_boot_base_revision_supported() == CLKS_FALSE) {
        clks_serial_write("[ERROR][BOOT] LIMINE BASE REVISION NOT SUPPORTED\n");
        clks_cpu_halt_forever();
    }

    boot_fb = clks_boot_get_framebuffer();

    if (boot_fb != CLKS_NULL) {
        clks_fb_init(boot_fb);
        clks_tty_init();
    }

    clks_log(CLKS_LOG_INFO, "BOOT", "CLEONOS STAGE1 START");

    if (boot_fb == CLKS_NULL) {
        clks_log(CLKS_LOG_WARN, "VIDEO", "NO FRAMEBUFFER FROM LIMINE");
    } else {
        clks_log_hex(CLKS_LOG_INFO, "VIDEO", "WIDTH", boot_fb->width);
        clks_log_hex(CLKS_LOG_INFO, "VIDEO", "HEIGHT", boot_fb->height);
        clks_log_hex(CLKS_LOG_INFO, "VIDEO", "PITCH", boot_fb->pitch);
        clks_log_hex(CLKS_LOG_INFO, "VIDEO", "BPP", boot_fb->bpp);
    }

#if defined(CLKS_ARCH_X86_64)
    clks_log(CLKS_LOG_INFO, "ARCH", "X86_64 ONLINE");
#elif defined(CLKS_ARCH_AARCH64)
    clks_log(CLKS_LOG_INFO, "ARCH", "AARCH64 ONLINE");
#endif

    clks_log(CLKS_LOG_INFO, "TTY", "VIRTUAL TTY0 READY");
    clks_log(CLKS_LOG_DEBUG, "KERNEL", "IDLE LOOP ENTER");

    clks_cpu_halt_forever();
}