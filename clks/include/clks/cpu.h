#ifndef CLKS_CPU_H
#define CLKS_CPU_H

#include <clks/compiler.h>

static inline void clks_cpu_pause(void) {
#if defined(CLKS_ARCH_X86_64)
    __asm__ volatile("pause");
#elif defined(CLKS_ARCH_AARCH64)
    __asm__ volatile("yield");
#endif
}

static inline CLKS_NORETURN void clks_cpu_halt_forever(void) {
    for (;;) {
#if defined(CLKS_ARCH_X86_64)
        __asm__ volatile("hlt");
#elif defined(CLKS_ARCH_AARCH64)
        __asm__ volatile("wfe");
#endif
    }
}

#endif