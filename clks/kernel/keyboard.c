#include <clks/exec.h>
#include <clks/keyboard.h>
#include <clks/log.h>
#include <clks/shell.h>
#include <clks/tty.h>
#include <clks/types.h>

#define CLKS_SC_ALT              0x38U
#define CLKS_SC_LSHIFT           0x2AU
#define CLKS_SC_RSHIFT           0x36U
#define CLKS_SC_F1               0x3BU
#define CLKS_SC_F2               0x3CU
#define CLKS_SC_F3               0x3DU
#define CLKS_SC_F4               0x3EU
#define CLKS_SC_EXT_PREFIX       0xE0U

#define CLKS_SC_EXT_HOME         0x47U
#define CLKS_SC_EXT_UP           0x48U
#define CLKS_SC_EXT_LEFT         0x4BU
#define CLKS_SC_EXT_RIGHT        0x4DU
#define CLKS_SC_EXT_END          0x4FU
#define CLKS_SC_EXT_DOWN         0x50U
#define CLKS_SC_EXT_DELETE       0x53U

#define CLKS_KBD_INPUT_CAP       256U
#define CLKS_KBD_TTY_MAX         8U
#define CLKS_KBD_DROP_LOG_EVERY  64ULL

static const char clks_kbd_map[128] = {
    [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5', [7] = '6', [8] = '7', [9] = '8',
    [10] = '9', [11] = '0', [12] = '-', [13] = '=', [14] = '\b', [15] = '\t',
    [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't', [21] = 'y', [22] = 'u', [23] = 'i',
    [24] = 'o', [25] = 'p', [26] = '[', [27] = ']', [28] = '\n',
    [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g', [35] = 'h', [36] = 'j', [37] = 'k',
    [38] = 'l', [39] = ';', [40] = '\'', [41] = '`', [43] = '\\',
    [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b', [49] = 'n', [50] = 'm',
    [51] = ',', [52] = '.', [53] = '/', [57] = ' '
};

static const char clks_kbd_shift_map[128] = {
    [2] = '!', [3] = '@', [4] = '#', [5] = '$', [6] = '%', [7] = '^', [8] = '&', [9] = '*',
    [10] = '(', [11] = ')', [12] = '_', [13] = '+', [14] = '\b', [15] = '\t',
    [16] = 'Q', [17] = 'W', [18] = 'E', [19] = 'R', [20] = 'T', [21] = 'Y', [22] = 'U', [23] = 'I',
    [24] = 'O', [25] = 'P', [26] = '{', [27] = '}', [28] = '\n',
    [30] = 'A', [31] = 'S', [32] = 'D', [33] = 'F', [34] = 'G', [35] = 'H', [36] = 'J', [37] = 'K',
    [38] = 'L', [39] = ':', [40] = '"', [41] = '~', [43] = '|',
    [44] = 'Z', [45] = 'X', [46] = 'C', [47] = 'V', [48] = 'B', [49] = 'N', [50] = 'M',
    [51] = '<', [52] = '>', [53] = '?', [57] = ' '
};

static char clks_kbd_input_queue[CLKS_KBD_TTY_MAX][CLKS_KBD_INPUT_CAP];
static u16 clks_kbd_input_head[CLKS_KBD_TTY_MAX];
static u16 clks_kbd_input_tail[CLKS_KBD_TTY_MAX];
static u16 clks_kbd_input_count[CLKS_KBD_TTY_MAX];

static clks_bool clks_kbd_alt_down = CLKS_FALSE;
static clks_bool clks_kbd_lshift_down = CLKS_FALSE;
static clks_bool clks_kbd_rshift_down = CLKS_FALSE;
static clks_bool clks_kbd_e0_prefix = CLKS_FALSE;
static u64 clks_kbd_hotkey_switches = 0ULL;

static u64 clks_kbd_push_count = 0ULL;
static u64 clks_kbd_pop_count = 0ULL;
static u64 clks_kbd_drop_count = 0ULL;

static u32 clks_keyboard_effective_tty_count(void) {
    u32 tty_count = clks_tty_count();

    if (tty_count == 0U) {
        return 1U;
    }

    if (tty_count > CLKS_KBD_TTY_MAX) {
        return CLKS_KBD_TTY_MAX;
    }

    return tty_count;
}

static u32 clks_keyboard_clamp_tty_index(u32 tty_index) {
    u32 tty_count = clks_keyboard_effective_tty_count();

    if (tty_index >= tty_count) {
        return 0U;
    }

    return tty_index;
}

static char clks_keyboard_translate_ext_scancode(u8 code) {
    switch (code) {
        case CLKS_SC_EXT_LEFT:
            return CLKS_KEY_LEFT;
        case CLKS_SC_EXT_RIGHT:
            return CLKS_KEY_RIGHT;
        case CLKS_SC_EXT_UP:
            return CLKS_KEY_UP;
        case CLKS_SC_EXT_DOWN:
            return CLKS_KEY_DOWN;
        case CLKS_SC_EXT_HOME:
            return CLKS_KEY_HOME;
        case CLKS_SC_EXT_END:
            return CLKS_KEY_END;
        case CLKS_SC_EXT_DELETE:
            return CLKS_KEY_DELETE;
        default:
            return '\0';
    }
}

static clks_bool clks_keyboard_queue_push_for_tty(u32 tty_index, char ch) {
    u32 tty = clks_keyboard_clamp_tty_index(tty_index);

    if (clks_kbd_input_count[tty] >= CLKS_KBD_INPUT_CAP) {
        clks_kbd_drop_count++;

        if ((clks_kbd_drop_count % CLKS_KBD_DROP_LOG_EVERY) == 1ULL) {
            clks_log(CLKS_LOG_WARN, "KBD", "INPUT QUEUE OVERFLOW");
            clks_log_hex(CLKS_LOG_WARN, "KBD", "TTY", (u64)tty);
            clks_log_hex(CLKS_LOG_WARN, "KBD", "DROPPED", clks_kbd_drop_count);
        }

        return CLKS_FALSE;
    }

    clks_kbd_input_queue[tty][clks_kbd_input_head[tty]] = ch;
    clks_kbd_input_head[tty] = (u16)((clks_kbd_input_head[tty] + 1U) % CLKS_KBD_INPUT_CAP);
    clks_kbd_input_count[tty]++;
    clks_kbd_push_count++;
    return CLKS_TRUE;
}

static clks_bool clks_keyboard_shell_input_enabled(void) {
    return (clks_tty_active() == 0U) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_keyboard_should_pump_shell_now(void) {
    if (clks_keyboard_shell_input_enabled() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_exec_is_running() == CLKS_TRUE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static char clks_keyboard_translate_scancode(u8 code) {
    clks_bool shift_active = (clks_kbd_lshift_down == CLKS_TRUE || clks_kbd_rshift_down == CLKS_TRUE)
                                 ? CLKS_TRUE
                                 : CLKS_FALSE;

    if (shift_active == CLKS_TRUE) {
        return clks_kbd_shift_map[code];
    }

    return clks_kbd_map[code];
}

void clks_keyboard_init(void) {
    u32 tty;

    for (tty = 0U; tty < CLKS_KBD_TTY_MAX; tty++) {
        clks_kbd_input_head[tty] = 0U;
        clks_kbd_input_tail[tty] = 0U;
        clks_kbd_input_count[tty] = 0U;
    }

    clks_kbd_alt_down = CLKS_FALSE;
    clks_kbd_lshift_down = CLKS_FALSE;
    clks_kbd_rshift_down = CLKS_FALSE;
    clks_kbd_e0_prefix = CLKS_FALSE;
    clks_kbd_hotkey_switches = 0ULL;
    clks_kbd_push_count = 0ULL;
    clks_kbd_pop_count = 0ULL;
    clks_kbd_drop_count = 0ULL;

    clks_log(CLKS_LOG_INFO, "KBD", "ALT+F1..F4 TTY HOTKEY ONLINE");
    clks_log(CLKS_LOG_INFO, "KBD", "PS2 INPUT QUEUE ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "KBD", "QUEUE_CAP", CLKS_KBD_INPUT_CAP);
}

void clks_keyboard_handle_scancode(u8 scancode) {
    clks_bool released;
    u8 code;

    if (scancode == CLKS_SC_EXT_PREFIX) {
        clks_kbd_e0_prefix = CLKS_TRUE;
        return;
    }

    released = ((scancode & 0x80U) != 0U) ? CLKS_TRUE : CLKS_FALSE;
    code = (u8)(scancode & 0x7FU);

    if (code == CLKS_SC_ALT) {
        clks_kbd_alt_down = (released == CLKS_FALSE) ? CLKS_TRUE : CLKS_FALSE;
        return;
    }

    if (code == CLKS_SC_LSHIFT) {
        clks_kbd_lshift_down = (released == CLKS_FALSE) ? CLKS_TRUE : CLKS_FALSE;
        return;
    }

    if (code == CLKS_SC_RSHIFT) {
        clks_kbd_rshift_down = (released == CLKS_FALSE) ? CLKS_TRUE : CLKS_FALSE;
        return;
    }

    if (released == CLKS_TRUE) {
        if (clks_kbd_e0_prefix == CLKS_TRUE) {
            clks_kbd_e0_prefix = CLKS_FALSE;
        }
        return;
    }

    if (clks_kbd_e0_prefix == CLKS_TRUE) {
        char ext = clks_keyboard_translate_ext_scancode(code);
        u32 active_tty = clks_tty_active();

        clks_kbd_e0_prefix = CLKS_FALSE;

        if (ext != '\0') {
            if (clks_keyboard_queue_push_for_tty(active_tty, ext) == CLKS_TRUE &&
                clks_keyboard_should_pump_shell_now() == CLKS_TRUE) {
                clks_shell_pump_input(1U);
            }
        }

        return;
    }

    if (clks_kbd_alt_down == CLKS_TRUE && code >= CLKS_SC_F1 && code <= CLKS_SC_F4) {
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

        return;
    }

    {
        char translated = clks_keyboard_translate_scancode(code);
        u32 active_tty = clks_tty_active();

        if (translated != '\0') {
            if (clks_keyboard_queue_push_for_tty(active_tty, translated) == CLKS_TRUE &&
                clks_keyboard_should_pump_shell_now() == CLKS_TRUE) {
                clks_shell_pump_input(1U);
            }
        }
    }
}

u64 clks_keyboard_hotkey_switch_count(void) {
    return clks_kbd_hotkey_switches;
}

clks_bool clks_keyboard_pop_char_for_tty(u32 tty_index, char *out_ch) {
    u32 tty = clks_keyboard_clamp_tty_index(tty_index);

    if (out_ch == CLKS_NULL || clks_kbd_input_count[tty] == 0U) {
        return CLKS_FALSE;
    }

    *out_ch = clks_kbd_input_queue[tty][clks_kbd_input_tail[tty]];
    clks_kbd_input_tail[tty] = (u16)((clks_kbd_input_tail[tty] + 1U) % CLKS_KBD_INPUT_CAP);
    clks_kbd_input_count[tty]--;
    clks_kbd_pop_count++;
    return CLKS_TRUE;
}

clks_bool clks_keyboard_pop_char(char *out_ch) {
    return clks_keyboard_pop_char_for_tty(clks_tty_active(), out_ch);
}

u64 clks_keyboard_buffered_count(void) {
    u64 total = 0ULL;
    u32 tty;

    for (tty = 0U; tty < CLKS_KBD_TTY_MAX; tty++) {
        total += (u64)clks_kbd_input_count[tty];
    }

    return total;
}

u64 clks_keyboard_drop_count(void) {
    return clks_kbd_drop_count;
}

u64 clks_keyboard_push_count(void) {
    return clks_kbd_push_count;
}

u64 clks_keyboard_pop_count(void) {
    return clks_kbd_pop_count;
}
