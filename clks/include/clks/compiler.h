#ifndef CLKS_COMPILER_H
#define CLKS_COMPILER_H

#define CLKS_USED __attribute__((used))
#define CLKS_NORETURN __attribute__((noreturn))
#define CLKS_PACKED __attribute__((packed))
#define CLKS_ALIGN(N) __attribute__((aligned(N)))

#if defined(CLKS_ARCH_X86_64) && defined(CLKS_ARCH_AARCH64)
#error "Only one architecture can be selected"
#endif

#if !defined(CLKS_ARCH_X86_64) && !defined(CLKS_ARCH_AARCH64)
#error "Missing architecture define: CLKS_ARCH_X86_64 or CLKS_ARCH_AARCH64"
#endif

#endif