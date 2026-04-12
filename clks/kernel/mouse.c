#include <clks/framebuffer.h>
#include <clks/log.h>
#include <clks/mouse.h>
#include <clks/types.h>

#define CLKS_PS2_DATA_PORT      0x60U
#define CLKS_PS2_STATUS_PORT    0x64U
#define CLKS_PS2_CMD_PORT       0x64U
#define CLKS_PS2_STATUS_OBF     0x01U
#define CLKS_PS2_STATUS_IBF     0x02U

#define CLKS_PS2_CMD_ENABLE_AUX 0xA8U
#define CLKS_PS2_CMD_READ_CFG   0x20U
#define CLKS_PS2_CMD_WRITE_CFG  0x60U
#define CLKS_PS2_CMD_WRITE_AUX  0xD4U

#define CLKS_PS2_MOUSE_CMD_RESET_DEFAULTS 0xF6U
#define CLKS_PS2_MOUSE_CMD_ENABLE_STREAM  0xF4U
#define CLKS_PS2_MOUSE_ACK                0xFAU

#define CLKS_MOUSE_IO_TIMEOUT      100000U
#define CLKS_MOUSE_DRAIN_MAX       64U
#define CLKS_MOUSE_SYNC_BIT        0x08U
#define CLKS_MOUSE_OVERFLOW_MASK   0xC0U
#define CLKS_MOUSE_BUTTON_MASK     0x07U

struct clks_mouse_runtime {
    i32 x;
    i32 y;
    u32 max_x;
    u32 max_y;
    u8 buttons;
    u8 packet[3];
    u8 packet_index;
    u64 packet_count;
    u64 drop_count;
    clks_bool ready;
};

static struct clks_mouse_runtime clks_mouse = {
    .x = 0,
    .y = 0,
    .max_x = 0U,
    .max_y = 0U,
    .buttons = 0U,
    .packet = {0U, 0U, 0U},
    .packet_index = 0U,
    .packet_count = 0ULL,
    .drop_count = 0ULL,
    .ready = CLKS_FALSE,
};

static inline void clks_mouse_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 clks_mouse_inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static clks_bool clks_mouse_wait_input_empty(void) {
    u32 i;

    for (i = 0U; i < CLKS_MOUSE_IO_TIMEOUT; i++) {
        if ((clks_mouse_inb(CLKS_PS2_STATUS_PORT) & CLKS_PS2_STATUS_IBF) == 0U) {
            return CLKS_TRUE;
        }
    }

    return CLKS_FALSE;
}

static clks_bool clks_mouse_wait_output_ready(void) {
    u32 i;

    for (i = 0U; i < CLKS_MOUSE_IO_TIMEOUT; i++) {
        if ((clks_mouse_inb(CLKS_PS2_STATUS_PORT) & CLKS_PS2_STATUS_OBF) != 0U) {
            return CLKS_TRUE;
        }
    }

    return CLKS_FALSE;
}

static clks_bool clks_mouse_write_cmd(u8 cmd) {
    if (clks_mouse_wait_input_empty() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_mouse_outb(CLKS_PS2_CMD_PORT, cmd);
    return CLKS_TRUE;
}

static clks_bool clks_mouse_write_data(u8 value) {
    if (clks_mouse_wait_input_empty() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_mouse_outb(CLKS_PS2_DATA_PORT, value);
    return CLKS_TRUE;
}

static clks_bool clks_mouse_read_data(u8 *out_value) {
    if (out_value == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_mouse_wait_output_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    *out_value = clks_mouse_inb(CLKS_PS2_DATA_PORT);
    return CLKS_TRUE;
}

static void clks_mouse_drain_output(void) {
    u32 i;

    for (i = 0U; i < CLKS_MOUSE_DRAIN_MAX; i++) {
        if ((clks_mouse_inb(CLKS_PS2_STATUS_PORT) & CLKS_PS2_STATUS_OBF) == 0U) {
            break;
        }

        (void)clks_mouse_inb(CLKS_PS2_DATA_PORT);
    }
}

static clks_bool clks_mouse_send_device_cmd(u8 cmd, u8 *out_ack) {
    u8 ack = 0U;

    if (clks_mouse_write_cmd(CLKS_PS2_CMD_WRITE_AUX) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_mouse_write_data(cmd) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_mouse_read_data(&ack) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (out_ack != CLKS_NULL) {
        *out_ack = ack;
    }

    return CLKS_TRUE;
}

static void clks_mouse_reset_runtime(void) {
    struct clks_framebuffer_info info;

    clks_mouse.x = 0;
    clks_mouse.y = 0;
    clks_mouse.max_x = 0U;
    clks_mouse.max_y = 0U;
    clks_mouse.buttons = 0U;
    clks_mouse.packet[0] = 0U;
    clks_mouse.packet[1] = 0U;
    clks_mouse.packet[2] = 0U;
    clks_mouse.packet_index = 0U;
    clks_mouse.packet_count = 0ULL;
    clks_mouse.drop_count = 0ULL;
    clks_mouse.ready = CLKS_FALSE;

    if (clks_fb_ready() == CLKS_TRUE) {
        info = clks_fb_info();

        if (info.width > 0U) {
            clks_mouse.max_x = info.width - 1U;
        }

        if (info.height > 0U) {
            clks_mouse.max_y = info.height - 1U;
        }

        clks_mouse.x = (i32)(clks_mouse.max_x / 2U);
        clks_mouse.y = (i32)(clks_mouse.max_y / 2U);
    }
}

void clks_mouse_init(void) {
    u8 config = 0U;
    u8 ack = 0U;

    clks_mouse_reset_runtime();
    clks_mouse_drain_output();

    if (clks_mouse_write_cmd(CLKS_PS2_CMD_ENABLE_AUX) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "MOUSE", "PS2 ENABLE AUX FAILED");
        return;
    }

    if (clks_mouse_write_cmd(CLKS_PS2_CMD_READ_CFG) == CLKS_FALSE ||
        clks_mouse_read_data(&config) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "MOUSE", "PS2 READ CFG FAILED");
        return;
    }

    config |= 0x02U;
    config &= (u8)~0x20U;

    if (clks_mouse_write_cmd(CLKS_PS2_CMD_WRITE_CFG) == CLKS_FALSE ||
        clks_mouse_write_data(config) == CLKS_FALSE) {
        clks_log(CLKS_LOG_WARN, "MOUSE", "PS2 WRITE CFG FAILED");
        return;
    }

    if (clks_mouse_send_device_cmd(CLKS_PS2_MOUSE_CMD_RESET_DEFAULTS, &ack) == CLKS_FALSE ||
        ack != CLKS_PS2_MOUSE_ACK) {
        clks_log(CLKS_LOG_WARN, "MOUSE", "PS2 RESET DEFAULTS FAILED");
        return;
    }

    if (clks_mouse_send_device_cmd(CLKS_PS2_MOUSE_CMD_ENABLE_STREAM, &ack) == CLKS_FALSE ||
        ack != CLKS_PS2_MOUSE_ACK) {
        clks_log(CLKS_LOG_WARN, "MOUSE", "PS2 ENABLE STREAM FAILED");
        return;
    }

    clks_mouse.ready = CLKS_TRUE;
    clks_log(CLKS_LOG_INFO, "MOUSE", "PS2 POINTER ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "MOUSE", "MAX_X", (u64)clks_mouse.max_x);
    clks_log_hex(CLKS_LOG_INFO, "MOUSE", "MAX_Y", (u64)clks_mouse.max_y);
}

void clks_mouse_handle_byte(u8 data_byte) {
    i32 dx;
    i32 dy;
    i32 next_x;
    i32 next_y;
    u8 status;

    if (clks_mouse.ready == CLKS_FALSE) {
        return;
    }

    if (clks_mouse.packet_index == 0U && (data_byte & CLKS_MOUSE_SYNC_BIT) == 0U) {
        clks_mouse.drop_count++;
        return;
    }

    clks_mouse.packet[clks_mouse.packet_index] = data_byte;
    clks_mouse.packet_index++;

    if (clks_mouse.packet_index < 3U) {
        return;
    }

    clks_mouse.packet_index = 0U;
    clks_mouse.packet_count++;

    status = clks_mouse.packet[0];
    clks_mouse.buttons = (u8)(status & CLKS_MOUSE_BUTTON_MASK);

    if ((status & CLKS_MOUSE_OVERFLOW_MASK) != 0U) {
        clks_mouse.drop_count++;
        return;
    }

    dx = (i32)((i8)clks_mouse.packet[1]);
    dy = (i32)((i8)clks_mouse.packet[2]);

    next_x = clks_mouse.x + dx;
    next_y = clks_mouse.y - dy;

    if (next_x < 0) {
        clks_mouse.x = 0;
    } else if ((u32)next_x > clks_mouse.max_x) {
        clks_mouse.x = (i32)clks_mouse.max_x;
    } else {
        clks_mouse.x = next_x;
    }

    if (next_y < 0) {
        clks_mouse.y = 0;
    } else if ((u32)next_y > clks_mouse.max_y) {
        clks_mouse.y = (i32)clks_mouse.max_y;
    } else {
        clks_mouse.y = next_y;
    }
}

void clks_mouse_snapshot(struct clks_mouse_state *out_state) {
    if (out_state == CLKS_NULL) {
        return;
    }

    out_state->x = clks_mouse.x;
    out_state->y = clks_mouse.y;
    out_state->buttons = clks_mouse.buttons;
    out_state->packet_count = clks_mouse.packet_count;
    out_state->ready = clks_mouse.ready;
}

clks_bool clks_mouse_ready(void) {
    return clks_mouse.ready;
}

u64 clks_mouse_packet_count(void) {
    return clks_mouse.packet_count;
}

u64 clks_mouse_drop_count(void) {
    return clks_mouse.drop_count;
}
