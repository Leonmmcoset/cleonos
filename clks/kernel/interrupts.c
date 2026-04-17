#include <clks/compiler.h>
#include <clks/cpu.h>
#include <clks/exec.h>
#include <clks/interrupts.h>
#include <clks/log.h>
#include <clks/keyboard.h>
#include <clks/mouse.h>
#include <clks/panic.h>
#include <clks/scheduler.h>
#include <clks/syscall.h>
#include <clks/types.h>

#define CLKS_IDT_ENTRY_COUNT 256U
#define CLKS_INTERRUPT_GATE  0x8EU
#define CLKS_USER_INT_GATE   0xEEU

#define CLKS_PIC1_CMD   0x20U
#define CLKS_PIC1_DATA  0x21U
#define CLKS_PIC2_CMD   0xA0U
#define CLKS_PIC2_DATA  0xA1U
#define CLKS_PIC_EOI    0x20U

#define CLKS_IRQ_BASE   32U
#define CLKS_IRQ_TIMER  32U
#define CLKS_IRQ_KEYBOARD 33U
#define CLKS_IRQ_MOUSE  44U
#define CLKS_IRQ_LAST   47U
#define CLKS_SYSCALL_VECTOR 128U

#define CLKS_PS2_DATA_PORT  0x60U
#define CLKS_PS2_STATUS_PORT 0x64U

struct clks_idt_entry {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} CLKS_PACKED;

struct clks_idtr {
    u16 limit;
    u64 base;
} CLKS_PACKED;

struct clks_interrupt_frame {
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 rbp;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 vector;
    u64 error_code;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

extern void clks_isr_stub_default(void);
extern void clks_isr_stub_0(void);
extern void clks_isr_stub_1(void);
extern void clks_isr_stub_2(void);
extern void clks_isr_stub_3(void);
extern void clks_isr_stub_4(void);
extern void clks_isr_stub_5(void);
extern void clks_isr_stub_6(void);
extern void clks_isr_stub_7(void);
extern void clks_isr_stub_8(void);
extern void clks_isr_stub_9(void);
extern void clks_isr_stub_10(void);
extern void clks_isr_stub_11(void);
extern void clks_isr_stub_12(void);
extern void clks_isr_stub_13(void);
extern void clks_isr_stub_14(void);
extern void clks_isr_stub_15(void);
extern void clks_isr_stub_16(void);
extern void clks_isr_stub_17(void);
extern void clks_isr_stub_18(void);
extern void clks_isr_stub_19(void);
extern void clks_isr_stub_20(void);
extern void clks_isr_stub_21(void);
extern void clks_isr_stub_22(void);
extern void clks_isr_stub_23(void);
extern void clks_isr_stub_24(void);
extern void clks_isr_stub_25(void);
extern void clks_isr_stub_26(void);
extern void clks_isr_stub_27(void);
extern void clks_isr_stub_28(void);
extern void clks_isr_stub_29(void);
extern void clks_isr_stub_30(void);
extern void clks_isr_stub_31(void);
extern void clks_isr_stub_32(void);
extern void clks_isr_stub_33(void);
extern void clks_isr_stub_34(void);
extern void clks_isr_stub_35(void);
extern void clks_isr_stub_36(void);
extern void clks_isr_stub_37(void);
extern void clks_isr_stub_38(void);
extern void clks_isr_stub_39(void);
extern void clks_isr_stub_40(void);
extern void clks_isr_stub_41(void);
extern void clks_isr_stub_42(void);
extern void clks_isr_stub_43(void);
extern void clks_isr_stub_44(void);
extern void clks_isr_stub_45(void);
extern void clks_isr_stub_46(void);
extern void clks_isr_stub_47(void);
extern void clks_isr_stub_128(void);

static struct clks_idt_entry clks_idt[CLKS_IDT_ENTRY_COUNT];
static u16 clks_idt_code_selector = 0x08U;
static u64 clks_timer_ticks = 0;

static const char *clks_exception_names[32] = {
    "DE DIVIDE ERROR",
    "DB DEBUG",
    "NMI",
    "BP BREAKPOINT",
    "OF OVERFLOW",
    "BR BOUND RANGE",
    "UD INVALID OPCODE",
    "NM DEVICE NOT AVAILABLE",
    "DF DOUBLE FAULT",
    "COPROCESSOR SEGMENT",
    "TS INVALID TSS",
    "NP SEGMENT NOT PRESENT",
    "SS STACK SEGMENT",
    "GP GENERAL PROTECTION",
    "PF PAGE FAULT",
    "RESERVED",
    "MF X87 FLOAT",
    "AC ALIGNMENT CHECK",
    "MC MACHINE CHECK",
    "XF SIMD FLOAT",
    "VE VIRT EXCEPTION",
    "CP CONTROL PROTECTION",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "HV HYPERVISOR",
    "VC VMM COMM",
    "SX SECURITY",
    "RESERVED"
};

static inline void clks_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 clks_inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void clks_io_wait(void) {
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

static void clks_pic_remap_and_mask(void) {
    u8 master_mask = clks_inb(CLKS_PIC1_DATA);
    u8 slave_mask = clks_inb(CLKS_PIC2_DATA);

    clks_outb(CLKS_PIC1_CMD, 0x11);
    clks_io_wait();
    clks_outb(CLKS_PIC2_CMD, 0x11);
    clks_io_wait();

    clks_outb(CLKS_PIC1_DATA, CLKS_IRQ_BASE);
    clks_io_wait();
    clks_outb(CLKS_PIC2_DATA, CLKS_IRQ_BASE + 8U);
    clks_io_wait();

    clks_outb(CLKS_PIC1_DATA, 4U);
    clks_io_wait();
    clks_outb(CLKS_PIC2_DATA, 2U);
    clks_io_wait();

    clks_outb(CLKS_PIC1_DATA, 0x01);
    clks_io_wait();
    clks_outb(CLKS_PIC2_DATA, 0x01);
    clks_io_wait();

    (void)master_mask;
    (void)slave_mask;

    clks_outb(CLKS_PIC1_DATA, 0xF8U);
    clks_outb(CLKS_PIC2_DATA, 0xEFU);
}

static void clks_pic_send_eoi(u64 vector) {
    if (vector >= 40U) {
        clks_outb(CLKS_PIC2_CMD, CLKS_PIC_EOI);
    }

    clks_outb(CLKS_PIC1_CMD, CLKS_PIC_EOI);
}

static void clks_idt_set_gate(u8 vector, void (*handler)(void), u8 flags) {
    u64 addr = (u64)handler;

    clks_idt[vector].offset_low = (u16)(addr & 0xFFFFULL);
    clks_idt[vector].selector = clks_idt_code_selector;
    clks_idt[vector].ist = 0;
    clks_idt[vector].type_attr = flags;
    clks_idt[vector].offset_mid = (u16)((addr >> 16) & 0xFFFFULL);
    clks_idt[vector].offset_high = (u32)((addr >> 32) & 0xFFFFFFFFULL);
    clks_idt[vector].zero = 0;
}

static void clks_load_idt(void) {
    struct clks_idtr idtr;

    idtr.limit = (u16)(sizeof(clks_idt) - 1U);
    idtr.base = (u64)&clks_idt[0];

    __asm__ volatile("lidt %0" : : "m"(idtr));
}


static clks_bool clks_ps2_has_output(void) {
    return (clks_inb(CLKS_PS2_STATUS_PORT) & 0x01U) != 0U ? CLKS_TRUE : CLKS_FALSE;
}
static void clks_enable_interrupts(void) {
    __asm__ volatile("sti");
}

void clks_interrupt_dispatch(struct clks_interrupt_frame *frame) {
    u64 vector = frame->vector;

    if (vector == CLKS_SYSCALL_VECTOR) {
        frame->rax = clks_syscall_dispatch((void *)frame);
        return;
    }

    if (vector < 32U) {
        if (clks_exec_handle_exception(vector,
                                       frame->error_code,
                                       frame->rip,
                                       &frame->rip,
                                       &frame->rdi,
                                       &frame->rsi) == CLKS_TRUE) {
            return;
        }

        clks_panic_exception(clks_exception_names[vector],
                             vector,
                             frame->error_code,
                             frame->rip,
                             frame->rbp,
                             frame->rsp);
    }

    if (vector == CLKS_IRQ_TIMER) {
        clks_timer_ticks++;
        clks_scheduler_on_timer_tick(clks_timer_ticks);
    } else if (vector == CLKS_IRQ_KEYBOARD) {
        if (clks_ps2_has_output() == CLKS_TRUE) {
            u8 scancode = clks_inb(CLKS_PS2_DATA_PORT);
            clks_keyboard_handle_scancode(scancode);
        }
    } else if (vector == CLKS_IRQ_MOUSE) {
        if (clks_ps2_has_output() == CLKS_TRUE) {
            u8 data_byte = clks_inb(CLKS_PS2_DATA_PORT);
            clks_mouse_handle_byte(data_byte);
        }
    }

    if (vector >= CLKS_IRQ_BASE && vector <= CLKS_IRQ_LAST) {
        (void)clks_exec_try_unwind_signaled_process(frame->rip, &frame->rip, &frame->rdi, &frame->rsi);
        clks_pic_send_eoi(vector);
    }
}

void clks_interrupts_init(void) {
    u32 i;

    __asm__ volatile("mov %%cs, %0" : "=r"(clks_idt_code_selector));

    for (i = 0; i < CLKS_IDT_ENTRY_COUNT; i++) {
        clks_idt_set_gate((u8)i, clks_isr_stub_default, CLKS_INTERRUPT_GATE);
    }

    clks_idt_set_gate(0, clks_isr_stub_0, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(1, clks_isr_stub_1, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(2, clks_isr_stub_2, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(3, clks_isr_stub_3, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(4, clks_isr_stub_4, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(5, clks_isr_stub_5, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(6, clks_isr_stub_6, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(7, clks_isr_stub_7, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(8, clks_isr_stub_8, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(9, clks_isr_stub_9, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(10, clks_isr_stub_10, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(11, clks_isr_stub_11, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(12, clks_isr_stub_12, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(13, clks_isr_stub_13, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(14, clks_isr_stub_14, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(15, clks_isr_stub_15, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(16, clks_isr_stub_16, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(17, clks_isr_stub_17, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(18, clks_isr_stub_18, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(19, clks_isr_stub_19, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(20, clks_isr_stub_20, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(21, clks_isr_stub_21, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(22, clks_isr_stub_22, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(23, clks_isr_stub_23, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(24, clks_isr_stub_24, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(25, clks_isr_stub_25, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(26, clks_isr_stub_26, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(27, clks_isr_stub_27, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(28, clks_isr_stub_28, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(29, clks_isr_stub_29, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(30, clks_isr_stub_30, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(31, clks_isr_stub_31, CLKS_INTERRUPT_GATE);

    clks_idt_set_gate(32, clks_isr_stub_32, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(33, clks_isr_stub_33, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(34, clks_isr_stub_34, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(35, clks_isr_stub_35, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(36, clks_isr_stub_36, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(37, clks_isr_stub_37, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(38, clks_isr_stub_38, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(39, clks_isr_stub_39, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(40, clks_isr_stub_40, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(41, clks_isr_stub_41, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(42, clks_isr_stub_42, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(43, clks_isr_stub_43, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(44, clks_isr_stub_44, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(45, clks_isr_stub_45, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(46, clks_isr_stub_46, CLKS_INTERRUPT_GATE);
    clks_idt_set_gate(47, clks_isr_stub_47, CLKS_INTERRUPT_GATE);

    clks_idt_set_gate(CLKS_SYSCALL_VECTOR, clks_isr_stub_128, CLKS_USER_INT_GATE);

    clks_pic_remap_and_mask();
    clks_load_idt();
    clks_enable_interrupts();
}

u64 clks_interrupts_timer_ticks(void) {
    return clks_timer_ticks;
}
