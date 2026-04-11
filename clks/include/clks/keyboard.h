#ifndef CLKS_KEYBOARD_H
#define CLKS_KEYBOARD_H

#include <clks/types.h>

void clks_keyboard_init(void);
void clks_keyboard_handle_scancode(u8 scancode);
u64 clks_keyboard_hotkey_switch_count(void);

#endif
