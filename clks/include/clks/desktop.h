#ifndef CLKS_DESKTOP_H
#define CLKS_DESKTOP_H

#include <clks/types.h>

void clks_desktop_init(void);
void clks_desktop_tick(u64 tick);
clks_bool clks_desktop_ready(void);

#endif
