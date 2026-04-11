#ifndef CLKS_PANIC_H
#define CLKS_PANIC_H

#include <clks/compiler.h>
#include <clks/types.h>

CLKS_NORETURN void clks_panic(const char *reason);
CLKS_NORETURN void clks_panic_exception(const char *name, u64 vector, u64 error_code, u64 rip);

#endif