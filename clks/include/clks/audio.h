#ifndef CLKS_AUDIO_H
#define CLKS_AUDIO_H

#include <clks/types.h>

void clks_audio_init(void);
clks_bool clks_audio_available(void);
clks_bool clks_audio_play_tone(u64 hz, u64 ticks);
void clks_audio_stop(void);
u64 clks_audio_play_count(void);

#endif