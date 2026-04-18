#ifndef CLKS_TTY_H
#define CLKS_TTY_H

#include <clks/types.h>

void clks_tty_init(void);
void clks_tty_write(const char *text);
void clks_tty_write_n(const char *text, usize len);
void clks_tty_write_char(char ch);
void clks_tty_switch(u32 tty_index);
void clks_tty_tick(u64 tick);
void clks_tty_scrollback_page_up(void);
void clks_tty_scrollback_page_down(void);
u32 clks_tty_active(void);
u32 clks_tty_count(void);
clks_bool clks_tty_ready(void);

#endif
