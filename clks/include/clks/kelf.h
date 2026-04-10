#ifndef CLKS_KELF_H
#define CLKS_KELF_H

#include <clks/types.h>

typedef u64 (*clks_kelf_entry_fn)(u64 tick, u64 run_count);

void clks_kelf_init(void);
void clks_kelf_tick(u64 tick);
u64 clks_kelf_count(void);
u64 clks_kelf_total_runs(void);

#endif
