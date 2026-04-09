#ifndef CLKS_INTERRUPTS_H
#define CLKS_INTERRUPTS_H

#include <clks/types.h>

void clks_interrupts_init(void);
u64 clks_interrupts_timer_ticks(void);

#endif