#ifndef CLKS_MOUSE_H
#define CLKS_MOUSE_H

#include <clks/types.h>

#define CLKS_MOUSE_BTN_LEFT   0x01U
#define CLKS_MOUSE_BTN_RIGHT  0x02U
#define CLKS_MOUSE_BTN_MIDDLE 0x04U

struct clks_mouse_state {
    i32 x;
    i32 y;
    u8 buttons;
    u64 packet_count;
    clks_bool ready;
};

void clks_mouse_init(void);
void clks_mouse_handle_byte(u8 data_byte);
void clks_mouse_snapshot(struct clks_mouse_state *out_state);
clks_bool clks_mouse_ready(void);
u64 clks_mouse_packet_count(void);
u64 clks_mouse_drop_count(void);

#endif
