#ifndef CLKS_KEYBOARD_H
#define CLKS_KEYBOARD_H

#include <clks/types.h>

#define CLKS_KEY_LEFT    ((char)0x01)
#define CLKS_KEY_RIGHT   ((char)0x02)
#define CLKS_KEY_UP      ((char)0x03)
#define CLKS_KEY_DOWN    ((char)0x04)
#define CLKS_KEY_HOME    ((char)0x05)
#define CLKS_KEY_END     ((char)0x06)
#define CLKS_KEY_DELETE  ((char)0x07)

void clks_keyboard_init(void);
void clks_keyboard_handle_scancode(u8 scancode);
u64 clks_keyboard_hotkey_switch_count(void);
clks_bool clks_keyboard_pop_char(char *out_ch);
clks_bool clks_keyboard_pop_char_for_tty(u32 tty_index, char *out_ch);
u64 clks_keyboard_buffered_count(void);
u64 clks_keyboard_drop_count(void);
u64 clks_keyboard_push_count(void);
u64 clks_keyboard_pop_count(void);

#endif
