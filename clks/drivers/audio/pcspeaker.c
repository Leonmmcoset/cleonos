#include <clks/audio.h>
#include <clks/exec.h>
#include <clks/log.h>
#include <clks/types.h>

#define CLKS_AUDIO_PIT_BASE_HZ 1193182ULL
#define CLKS_AUDIO_FREQ_MIN      20ULL
#define CLKS_AUDIO_FREQ_MAX   20000ULL
#define CLKS_AUDIO_TICKS_MAX   2048ULL

static clks_bool clks_audio_ready = CLKS_FALSE;
static u64 clks_audio_played_count = 0ULL;

#if defined(CLKS_ARCH_X86_64)
static inline void clks_audio_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 clks_audio_inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void clks_audio_program_pc_speaker(u64 hz) {
    u64 divisor64 = CLKS_AUDIO_PIT_BASE_HZ / hz;
    u16 divisor;
    u8 control;

    if (divisor64 == 0ULL) {
        divisor64 = 1ULL;
    }

    if (divisor64 > 0xFFFFULL) {
        divisor64 = 0xFFFFULL;
    }

    divisor = (u16)divisor64;
    clks_audio_outb(0x43U, 0xB6U);
    clks_audio_outb(0x42U, (u8)(divisor & 0xFFU));
    clks_audio_outb(0x42U, (u8)((divisor >> 8) & 0xFFU));

    control = clks_audio_inb(0x61U);
    clks_audio_outb(0x61U, (u8)(control | 0x03U));
}
#endif

static u64 clks_audio_clamp_hz(u64 hz) {
    if (hz < CLKS_AUDIO_FREQ_MIN) {
        return CLKS_AUDIO_FREQ_MIN;
    }

    if (hz > CLKS_AUDIO_FREQ_MAX) {
        return CLKS_AUDIO_FREQ_MAX;
    }

    return hz;
}

void clks_audio_init(void) {
#if defined(CLKS_ARCH_X86_64)
    clks_audio_ready = CLKS_TRUE;
    clks_audio_played_count = 0ULL;
    clks_audio_stop();

    clks_log(CLKS_LOG_INFO, "AUDIO", "PC SPEAKER ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "AUDIO", "PIT_BASE_HZ", CLKS_AUDIO_PIT_BASE_HZ);
#else
    clks_audio_ready = CLKS_FALSE;
    clks_audio_played_count = 0ULL;
    clks_log(CLKS_LOG_WARN, "AUDIO", "AUDIO OUTPUT NOT AVAILABLE ON THIS ARCH");
#endif
}

clks_bool clks_audio_available(void) {
    return clks_audio_ready;
}

clks_bool clks_audio_play_tone(u64 hz, u64 ticks) {
    if (clks_audio_ready == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (ticks == 0ULL) {
        return CLKS_TRUE;
    }

    if (ticks > CLKS_AUDIO_TICKS_MAX) {
        ticks = CLKS_AUDIO_TICKS_MAX;
    }

    if (hz == 0ULL) {
        clks_audio_stop();
        (void)clks_exec_sleep_ticks(ticks);
        return CLKS_TRUE;
    }

    hz = clks_audio_clamp_hz(hz);

#if defined(CLKS_ARCH_X86_64)
    clks_audio_program_pc_speaker(hz);
    (void)clks_exec_sleep_ticks(ticks);
    clks_audio_stop();
    clks_audio_played_count++;
    return CLKS_TRUE;
#else
    return CLKS_FALSE;
#endif
}

void clks_audio_stop(void) {
#if defined(CLKS_ARCH_X86_64)
    u8 control;

    if (clks_audio_ready == CLKS_FALSE) {
        return;
    }

    control = clks_audio_inb(0x61U);
    clks_audio_outb(0x61U, (u8)(control & 0xFCU));
#endif
}

u64 clks_audio_play_count(void) {
    return clks_audio_played_count;
}