#include "cmd_runtime.h"

#define USH_WAVPLAY_FILE_MAX       65536ULL
#define USH_WAVPLAY_DEFAULT_STEPS    256ULL
#define USH_WAVPLAY_MAX_STEPS       4096ULL
#define USH_WAVPLAY_DEFAULT_TICKS      1ULL
#define USH_WAVPLAY_MAX_TICKS         64ULL
#define USH_WAVPLAY_RUN_TICK_MAX     512ULL

typedef struct ush_wav_info {
    const unsigned char *data;
    u64 data_size;
    u64 frame_count;
    u64 sample_rate;
    u64 channels;
    u64 bits_per_sample;
    u64 block_align;
} ush_wav_info;

static unsigned char ush_wavplay_file_buf[USH_WAVPLAY_FILE_MAX + 1ULL];

static unsigned int ush_wav_le16(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U);
}

static unsigned int ush_wav_le32(const unsigned char *ptr) {
    return (unsigned int)ptr[0] |
           ((unsigned int)ptr[1] << 8U) |
           ((unsigned int)ptr[2] << 16U) |
           ((unsigned int)ptr[3] << 24U);
}

static int ush_wav_tag_eq(const unsigned char *tag, const char *lit4) {
    if (tag == (const unsigned char *)0 || lit4 == (const char *)0) {
        return 0;
    }

    return (tag[0] == (unsigned char)lit4[0] &&
            tag[1] == (unsigned char)lit4[1] &&
            tag[2] == (unsigned char)lit4[2] &&
            tag[3] == (unsigned char)lit4[3])
               ? 1
               : 0;
}

static void ush_wavplay_write_u64_dec(u64 value) {
    char rev[32];
    u64 len = 0ULL;

    if (value == 0ULL) {
        ush_write_char('0');
        return;
    }

    while (value > 0ULL && len < (u64)sizeof(rev)) {
        rev[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0ULL) {
        len--;
        ush_write_char(rev[len]);
    }
}

static void ush_wavplay_print_meta(const char *path, const ush_wav_info *info, u64 steps, u64 ticks) {
    ush_write("wavplay: ");
    ush_writeln(path);

    ush_write("  sample_rate=");
    ush_wavplay_write_u64_dec(info->sample_rate);
    ush_write("Hz  channels=");
    ush_wavplay_write_u64_dec(info->channels);
    ush_write("  bits=");
    ush_wavplay_write_u64_dec(info->bits_per_sample);
    ush_write("  frames=");
    ush_wavplay_write_u64_dec(info->frame_count);
    ush_write_char('\n');

    ush_write("  play_steps=");
    ush_wavplay_write_u64_dec(steps);
    ush_write("  ticks_per_step=");
    ush_wavplay_write_u64_dec(ticks);
    ush_write_char('\n');
}

static int ush_wav_parse(const unsigned char *buffer, u64 size, ush_wav_info *out_info) {
    u64 offset = 12ULL;
    int found_fmt = 0;
    int found_data = 0;
    u64 sample_rate = 0ULL;
    u64 channels = 0ULL;
    u64 bits = 0ULL;
    u64 block_align = 0ULL;
    const unsigned char *data = (const unsigned char *)0;
    u64 data_size = 0ULL;

    if (buffer == (const unsigned char *)0 || out_info == (ush_wav_info *)0) {
        return 0;
    }

    if (size < 12ULL) {
        return 0;
    }

    if (ush_wav_tag_eq(&buffer[0], "RIFF") == 0 || ush_wav_tag_eq(&buffer[8], "WAVE") == 0) {
        return 0;
    }

    while (offset + 8ULL <= size) {
        const unsigned char *chunk_tag = &buffer[offset];
        u64 chunk_size = (u64)ush_wav_le32(&buffer[offset + 4ULL]);
        u64 chunk_data = offset + 8ULL;
        u64 next_offset;

        if (chunk_data > size || chunk_size > (size - chunk_data)) {
            return 0;
        }

        if (ush_wav_tag_eq(chunk_tag, "fmt ") != 0) {
            if (chunk_size < 16ULL) {
                return 0;
            }

            if (ush_wav_le16(&buffer[chunk_data + 0ULL]) != 1U) {
                return 0;
            }

            channels = (u64)ush_wav_le16(&buffer[chunk_data + 2ULL]);
            sample_rate = (u64)ush_wav_le32(&buffer[chunk_data + 4ULL]);
            block_align = (u64)ush_wav_le16(&buffer[chunk_data + 12ULL]);
            bits = (u64)ush_wav_le16(&buffer[chunk_data + 14ULL]);
            found_fmt = 1;
        } else if (ush_wav_tag_eq(chunk_tag, "data") != 0) {
            data = &buffer[chunk_data];
            data_size = chunk_size;
            found_data = 1;
        }

        next_offset = chunk_data + chunk_size;

        if ((chunk_size & 1ULL) != 0ULL) {
            next_offset++;
        }

        if (next_offset <= offset || next_offset > size) {
            break;
        }

        offset = next_offset;
    }

    if (found_fmt == 0 || found_data == 0) {
        return 0;
    }

    if ((channels != 1ULL && channels != 2ULL) || (bits != 8ULL && bits != 16ULL)) {
        return 0;
    }

    if (sample_rate == 0ULL || block_align == 0ULL) {
        return 0;
    }

    if (data_size < block_align) {
        return 0;
    }

    out_info->data = data;
    out_info->data_size = data_size;
    out_info->sample_rate = sample_rate;
    out_info->channels = channels;
    out_info->bits_per_sample = bits;
    out_info->block_align = block_align;
    out_info->frame_count = data_size / block_align;

    return (out_info->frame_count > 0ULL) ? 1 : 0;
}

static u64 ush_wav_sample_deviation(const ush_wav_info *info, u64 frame_index) {
    const unsigned char *frame;

    if (info == (const ush_wav_info *)0 || info->data == (const unsigned char *)0 || info->frame_count == 0ULL) {
        return 0ULL;
    }

    if (frame_index >= info->frame_count) {
        frame_index = info->frame_count - 1ULL;
    }

    frame = &info->data[frame_index * info->block_align];

    if (info->bits_per_sample == 8ULL) {
        unsigned int left = (unsigned int)frame[0];
        unsigned int dev_left = (left >= 128U) ? (left - 128U) : (128U - left);

        if (info->channels == 2ULL && info->block_align >= 2ULL) {
            unsigned int right = (unsigned int)frame[1];
            unsigned int dev_right = (right >= 128U) ? (right - 128U) : (128U - right);
            return (u64)((dev_left + dev_right) / 2U);
        }

        return (u64)dev_left;
    }

    if (info->bits_per_sample == 16ULL) {
        unsigned int raw_left;
        int sample_left;
        unsigned int dev_left;

        if (info->block_align < 2ULL) {
            return 0ULL;
        }

        raw_left = ush_wav_le16(frame);
        sample_left = (raw_left >= 32768U) ? ((int)raw_left - 65536) : (int)raw_left;
        if (sample_left < 0) {
            sample_left = -sample_left;
        }
        dev_left = (unsigned int)((unsigned int)sample_left >> 8U);

        if (info->channels == 2ULL && info->block_align >= 4ULL) {
            unsigned int raw_right = ush_wav_le16(frame + 2ULL);
            int sample_right = (raw_right >= 32768U) ? ((int)raw_right - 65536) : (int)raw_right;
            unsigned int dev_right;

            if (sample_right < 0) {
                sample_right = -sample_right;
            }

            dev_right = (unsigned int)((unsigned int)sample_right >> 8U);
            return (u64)((dev_left + dev_right) / 2U);
        }

        return (u64)dev_left;
    }

    return 0ULL;
}

static int ush_wavplay_parse_args(const char *arg,
                                  char *out_path,
                                  u64 out_path_size,
                                  u64 *out_steps,
                                  u64 *out_ticks,
                                  int *out_stop) {
    char first[USH_PATH_MAX];
    char second[32];
    char third[32];
    const char *rest = "";
    const char *rest2 = "";
    const char *rest3 = "";

    if (out_path == (char *)0 || out_steps == (u64 *)0 || out_ticks == (u64 *)0 || out_stop == (int *)0) {
        return 0;
    }

    *out_steps = USH_WAVPLAY_DEFAULT_STEPS;
    *out_ticks = USH_WAVPLAY_DEFAULT_TICKS;
    *out_stop = 0;
    out_path[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--stop") != 0 || ush_streq(first, "stop") != 0) {
        if (rest != (const char *)0 && rest[0] != '\0') {
            return 0;
        }

        *out_stop = 1;
        return 1;
    }

    ush_copy(out_path, out_path_size, first);

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            return 0;
        }

        if (ush_parse_u64_dec(second, out_steps) == 0 || *out_steps == 0ULL || *out_steps > USH_WAVPLAY_MAX_STEPS) {
            return 0;
        }

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            if (ush_split_first_and_rest(rest2, third, (u64)sizeof(third), &rest3) == 0) {
                return 0;
            }

            if (ush_parse_u64_dec(third, out_ticks) == 0 || *out_ticks == 0ULL || *out_ticks > USH_WAVPLAY_MAX_TICKS) {
                return 0;
            }

            if (rest3 != (const char *)0 && rest3[0] != '\0') {
                return 0;
            }
        }
    }

    return 1;
}

static int ush_cmd_wavplay(const ush_state *sh, const char *arg) {
    char path_arg[USH_PATH_MAX];
    char abs_path[USH_PATH_MAX];
    ush_wav_info info;
    u64 file_size;
    u64 got;
    u64 steps;
    u64 ticks_per_step;
    u64 stride;
    u64 i;
    u64 run_freq = 0ULL;
    u64 run_ticks = 0ULL;
    int stop_only;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_wavplay_parse_args(arg, path_arg, (u64)sizeof(path_arg), &steps, &ticks_per_step, &stop_only) == 0) {
        ush_writeln("wavplay: usage wavplay <file.wav> [steps<=4096] [ticks<=64]");
        ush_writeln("wavplay: usage wavplay --stop");
        return 0;
    }

    if (stop_only != 0) {
        (void)cleonos_sys_audio_stop();
        ush_writeln("wavplay: stopped");
        return 1;
    }

    if (cleonos_sys_audio_available() == 0ULL) {
        ush_writeln("wavplay: audio device unavailable");
        return 0;
    }

    if (ush_resolve_path(sh, path_arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln("wavplay: invalid path");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(abs_path) != 1ULL) {
        ush_writeln("wavplay: file not found");
        return 0;
    }

    file_size = cleonos_sys_fs_stat_size(abs_path);

    if (file_size == (u64)-1 || file_size == 0ULL) {
        ush_writeln("wavplay: empty or unreadable file");
        return 0;
    }

    if (file_size > USH_WAVPLAY_FILE_MAX) {
        ush_writeln("wavplay: file too large (max 65536 bytes)");
        return 0;
    }

    got = cleonos_sys_fs_read(abs_path, (char *)ush_wavplay_file_buf, file_size);

    if (got != file_size) {
        ush_writeln("wavplay: read failed");
        return 0;
    }

    if (ush_wav_parse(ush_wavplay_file_buf, got, &info) == 0) {
        ush_writeln("wavplay: unsupported wav (need PCM 8/16-bit, mono/stereo)");
        return 0;
    }

    if (steps > info.frame_count) {
        steps = info.frame_count;
    }

    if (steps == 0ULL) {
        ush_writeln("wavplay: nothing to play");
        return 0;
    }

    if (steps < 8ULL) {
        steps = 8ULL;

        if (steps > info.frame_count) {
            steps = info.frame_count;
        }
    }

    ush_wavplay_print_meta(abs_path, &info, steps, ticks_per_step);

    stride = info.frame_count / steps;
    if (stride == 0ULL) {
        stride = 1ULL;
    }

    for (i = 0ULL; i < steps; i++) {
        u64 frame_index = i * stride;
        u64 deviation;
        u64 freq;

        if (frame_index >= info.frame_count) {
            frame_index = info.frame_count - 1ULL;
        }

        deviation = ush_wav_sample_deviation(&info, frame_index);

        if (deviation < 4ULL) {
            freq = 0ULL;
        } else {
            freq = 180ULL + (deviation * 12ULL);
        }

        if (run_ticks == 0ULL) {
            run_freq = freq;
            run_ticks = ticks_per_step;
            continue;
        }

        if (freq == run_freq && (run_ticks + ticks_per_step) <= USH_WAVPLAY_RUN_TICK_MAX) {
            run_ticks += ticks_per_step;
            continue;
        }

        if (cleonos_sys_audio_play_tone(run_freq, run_ticks) == 0ULL) {
            ush_writeln("wavplay: playback failed");
            (void)cleonos_sys_audio_stop();
            return 0;
        }

        run_freq = freq;
        run_ticks = ticks_per_step;
    }

    if (run_ticks > 0ULL) {
        if (cleonos_sys_audio_play_tone(run_freq, run_ticks) == 0ULL) {
            ush_writeln("wavplay: playback failed");
            (void)cleonos_sys_audio_stop();
            return 0;
        }
    }

    (void)cleonos_sys_audio_stop();
    ush_writeln("wavplay: done");
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "wavplay") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_wavplay(&sh, arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}