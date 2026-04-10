#include <clks/boot.h>
#include <clks/elf64.h>
#include <clks/elfrunner.h>
#include <clks/log.h>
#include <clks/types.h>

static clks_bool clks_elfrunner_ready = CLKS_FALSE;

void clks_elfrunner_init(void) {
    clks_elfrunner_ready = CLKS_TRUE;
    clks_log(CLKS_LOG_INFO, "ELF", "ELFRUNNER FRAMEWORK ONLINE");
}

clks_bool clks_elfrunner_probe_kernel_executable(void) {
    const struct limine_file *exe = clks_boot_get_executable_file();
    struct clks_elf64_info info;

    if (clks_elfrunner_ready == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (exe == CLKS_NULL || exe->address == CLKS_NULL || exe->size == 0ULL) {
        clks_log(CLKS_LOG_ERROR, "ELF", "NO EXECUTABLE FILE FROM LIMINE");
        return CLKS_FALSE;
    }

    if (clks_elf64_inspect(exe->address, exe->size, &info) == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "ELF", "KERNEL ELF INSPECT FAILED");
        return CLKS_FALSE;
    }

    clks_log_hex(CLKS_LOG_INFO, "ELF", "ENTRY", info.entry);
    clks_log_hex(CLKS_LOG_INFO, "ELF", "PHNUM", info.phnum);
    clks_log_hex(CLKS_LOG_INFO, "ELF", "LOAD_SEGMENTS", info.loadable_segments);
    clks_log_hex(CLKS_LOG_INFO, "ELF", "TOTAL_MEMSZ", info.total_load_memsz);

    return CLKS_TRUE;
}