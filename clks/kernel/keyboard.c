#include <clks/keyboard.h>
#include <clks/log.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_SC_ALT        0x38U
#define CLKS_SC_F1         0x3BU
#define CLKS_SC_F2         0x3CU
#define CLKS_SC_F3         0x3DU
#define CLKS_SC_F4         0x3EU

static clks_bool clks_kbd_alt_down = CLKS_FALSE;
static u64 clks_kbd_hotkey_switches = 0ULL;

void clks_keyboard_init(void) {
    clks_kbd_alt_down = CLKS_FALSE;
    clks_kbd_hotkey_switches = 0ULL;
    clks_log(CLKS_LOG_INFO, "KBD", "ALT+F1..F4 TTY HOTKEY ONLINE");
}

void clks_keyboard_handle_scancode(u8 scancode) {
    clks_bool released = ((scancode & 0x80U) != 0U) ? CLKS_TRUE : CLKS_FALSE;
    u8 code = (u8)(scancode & 0x7FU);

    if (code == CLKS_SC_ALT) {
        clks_kbd_alt_down = (released == CLKS_FALSE) ? CLKS_TRUE : CLKS_FALSE;
        return;
    }

    if (released == CLKS_TRUE || clks_kbd_alt_down == CLKS_FALSE) {
        return;
    }

    if (code >= CLKS_SC_F1 && code <= CLKS_SC_F4) {
        u32 target = (u32)(code - CLKS_SC_F1);
        u32 before = clks_tty_active();
        u32 after;

        clks_tty_switch(target);
        after = clks_tty_active();

        if (after != before) {
            clks_kbd_hotkey_switches++;
            clks_log(CLKS_LOG_INFO, "TTY", "HOTKEY SWITCH");
            clks_log_hex(CLKS_LOG_INFO, "TTY", "ACTIVE", (u64)after);
            clks_log_hex(CLKS_LOG_INFO, "TTY", "HOTKEY_SWITCHES", clks_kbd_hotkey_switches);
        }
    }
}

u64 clks_keyboard_hotkey_switch_count(void) {
    return clks_kbd_hotkey_switches;
}
