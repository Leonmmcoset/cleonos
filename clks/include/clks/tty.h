#ifndef CLKS_TTY_H
#define CLKS_TTY_H

#include <clks/types.h>

void clks_tty_init(void);
void clks_tty_write(const char *text);
void clks_tty_write_char(char ch);
void clks_tty_switch(u32 tty_index);
u32 clks_tty_active(void);

#endif