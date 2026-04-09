#include <clks/cpu.h>
#include <clks/kernel.h>

void _start(void) {
    clks_kernel_main();
    clks_cpu_halt_forever();
}