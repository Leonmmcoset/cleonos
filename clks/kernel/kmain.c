#include <clks/boot.h>
#include <clks/cpu.h>
#include <clks/driver.h>
#include <clks/elfrunner.h>
#include <clks/framebuffer.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/interrupts.h>
#include <clks/kelf.h>
#include <clks/kernel.h>
#include <clks/log.h>
#include <clks/pmm.h>
#include <clks/scheduler.h>
#include <clks/serial.h>
#include <clks/service.h>
#include <clks/syscall.h>
#include <clks/tty.h>
#include <clks/types.h>
#include <clks/userland.h>

static void clks_task_klogd(u64 tick) {
    static u64 last_emit = 0ULL;

    clks_service_heartbeat(CLKS_SERVICE_LOG, tick);

    if (tick - last_emit >= 1000ULL) {
        clks_log_hex(CLKS_LOG_DEBUG, "TASK", "KLOGD_TICK", tick);
        last_emit = tick;
    }
}

static void clks_task_kworker(u64 tick) {
    static u32 phase = 0U;

    clks_service_heartbeat(CLKS_SERVICE_SCHED, tick);

    switch (phase) {
        case 0U:
            clks_service_heartbeat(CLKS_SERVICE_MEM, tick);
            break;
        case 1U:
            clks_service_heartbeat(CLKS_SERVICE_FS, tick);
            break;
        case 2U:
            clks_service_heartbeat(CLKS_SERVICE_DRIVER, tick);
            break;
        default:
            clks_service_heartbeat(CLKS_SERVICE_LOG, tick);
            break;
    }

    phase = (phase + 1U) & 3U;
}

static void clks_task_kelfd(u64 tick) {
    clks_service_heartbeat(CLKS_SERVICE_KELF, tick);
    clks_kelf_tick(tick);
}

void clks_kernel_main(void) {
    const struct limine_framebuffer *boot_fb;
    const struct limine_memmap_response *boot_memmap;
    struct clks_pmm_stats pmm_stats;
    struct clks_heap_stats heap_stats;
    struct clks_scheduler_stats sched_stats;
    struct clks_fs_node_info fs_system_dir = {0};
    void *heap_probe = CLKS_NULL;
    u64 syscall_ticks;
    u64 fs_root_children;

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

    clks_log(CLKS_LOG_INFO, "BOOT", "CLEONOS STAGE10 START");

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

    boot_memmap = clks_boot_get_memmap();

    if (boot_memmap == CLKS_NULL) {
        clks_log(CLKS_LOG_ERROR, "MEM", "NO LIMINE MEMMAP RESPONSE");
        clks_cpu_halt_forever();
    }

    clks_pmm_init(boot_memmap);
    pmm_stats = clks_pmm_get_stats();

    clks_log_hex(CLKS_LOG_INFO, "PMM", "MANAGED_PAGES", pmm_stats.managed_pages);
    clks_log_hex(CLKS_LOG_INFO, "PMM", "FREE_PAGES", pmm_stats.free_pages);
    clks_log_hex(CLKS_LOG_INFO, "PMM", "USED_PAGES", pmm_stats.used_pages);
    clks_log_hex(CLKS_LOG_INFO, "PMM", "DROPPED_PAGES", pmm_stats.dropped_pages);

    clks_heap_init();
    heap_stats = clks_heap_get_stats();

    clks_log_hex(CLKS_LOG_INFO, "HEAP", "TOTAL_BYTES", heap_stats.total_bytes);
    clks_log_hex(CLKS_LOG_INFO, "HEAP", "FREE_BYTES", heap_stats.free_bytes);

    heap_probe = clks_kmalloc(128);

    if (heap_probe == CLKS_NULL) {
        clks_log(CLKS_LOG_ERROR, "HEAP", "KMALLOC SELFTEST FAILED");
    } else {
        clks_log(CLKS_LOG_INFO, "HEAP", "KMALLOC SELFTEST OK");
        clks_kfree(heap_probe);
    }

    clks_fs_init();

    if (clks_fs_is_ready() == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "FS", "RAMDISK FS INIT FAILED");
        clks_cpu_halt_forever();
    }

    fs_root_children = clks_fs_count_children("/");
    clks_log_hex(CLKS_LOG_INFO, "FS", "ROOT_CHILDREN", fs_root_children);

    if (clks_fs_stat("/system", &fs_system_dir) == CLKS_FALSE || fs_system_dir.type != CLKS_FS_NODE_DIR) {
        clks_log(CLKS_LOG_ERROR, "FS", "/SYSTEM DIRECTORY CHECK FAILED");
        clks_cpu_halt_forever();
    }

    if (clks_userland_init() == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "USER", "USERLAND INIT FAILED");
        clks_cpu_halt_forever();
    }

    clks_driver_init();
    clks_kelf_init();

    clks_scheduler_init();

    if (clks_scheduler_add_kernel_task_ex("klogd", 4U, clks_task_klogd) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "SCHED", "FAILED TO ADD KLOGD TASK");
    }

    if (clks_scheduler_add_kernel_task_ex("kworker", 3U, clks_task_kworker) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "SCHED", "FAILED TO ADD KWORKER TASK");
    }

    if (clks_scheduler_add_kernel_task_ex("kelfd", 5U, clks_task_kelfd) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "SCHED", "FAILED TO ADD KELFD TASK");
    }

    sched_stats = clks_scheduler_get_stats();
    clks_log_hex(CLKS_LOG_INFO, "SCHED", "TASK_COUNT", sched_stats.task_count);

    clks_service_init();

    clks_elfrunner_init();

    if (clks_elfrunner_probe_kernel_executable() == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "ELF", "KERNEL ELF PROBE FAILED");
    }

    clks_syscall_init();

    clks_interrupts_init();
    clks_log(CLKS_LOG_INFO, "INT", "IDT + PIC INITIALIZED");

    syscall_ticks = clks_syscall_invoke_kernel(CLKS_SYSCALL_TIMER_TICKS, 0ULL, 0ULL, 0ULL);
    clks_log_hex(CLKS_LOG_INFO, "SYSCALL", "TICKS", syscall_ticks);

    clks_log(CLKS_LOG_INFO, "TTY", "VIRTUAL TTY0 READY");
    clks_log(CLKS_LOG_DEBUG, "KERNEL", "IDLE LOOP ENTER");

    clks_cpu_halt_forever();
}
