#ifndef CLKS_SHELL_H
#define CLKS_SHELL_H

#include <clks/types.h>

void clks_shell_init(void);
void clks_shell_pump_input(u32 max_chars);
void clks_shell_tick(u64 tick);

#endif