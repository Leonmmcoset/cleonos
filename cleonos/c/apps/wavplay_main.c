#include "cmd_runtime.h"

#define USH_WAVPLAY_DEFAULT_STEPS 256ULL
#define USH_WAVPLAY_MAX_STEPS 4096ULL
#define USH_WAVPLAY_DEFAULT_TICKS 1ULL
#define USH_WAVPLAY_MAX_TICKS 64ULL
#define USH_WAVPLAY_RUN_TICK_MAX 512ULL

typedef struct ush_wav_info {
    u64 data_size;
    u64 frame_count;
    u64 sample_rate;
    u64 channels;
    u64 bits_per_sample;
    u64 block_align;
} ush_wav_info;

static unsigned int ush_wav_le16(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U);
}

static unsigned int ush_wav_le32(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U) | ((unsigned int)ptr[2] << 16U) |
           ((unsigned int)ptr[3] << 24U);
}

static int ush_wav_tag_eq(const unsigned char *tag, const char *lit4) {
    if (tag == (const unsigned char *)0 || lit4 == (const char *)0) {
        return 0;
    }

    return (tag[0] == (unsigned char)lit4[0] && tag[1] == (unsigned char)lit4[1] && tag[2] == (unsigned char)lit4[2] &&
            tag[3] == (unsigned char)lit4[3])
               ? 1
               : 0;
}

static int ush_wav_read_exact(u64 fd, unsigned char *out, u64 size) {
    u64 done = 0ULL;

    if (out == (unsigned char *)0 || size == 0ULL) {
        return 0;
    }

    while (done < size) {
        u64 got = cleonos_sys_fd_read(fd, out + done, size - done);

        if (got == (u64)-1 || got == 0ULL) {
            return 0;
        }

        done += got;
    }

    return 1;
}

static int ush_wav_skip_bytes(u64 fd, u64 size) {
    unsigned char scratch[256];
    u64 remaining = size;

    while (remaining > 0ULL) {
        u64 req = remaining;
        u64 got;

        if (req > (u64)sizeof(scratch)) {
            req = (u64)sizeof(scratch);
        }

        got = cleonos_sys_fd_read(fd, scratch, req);

        if (got == (u64)-1 || got == 0ULL) {
            return 0;
        }

        remaining -= got;
    }

    return 1;
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

static int ush_wav_parse_stream(u64 fd, ush_wav_info *out_info) {
    unsigned char riff_header[12];
    unsigned char chunk_header[8];
    unsigned char fmt_min[16];
    int found_fmt = 0;
    int found_data = 0;
    u64 sample_rate = 0ULL;
    u64 channels = 0ULL;
    u64 bits = 0ULL;
    u64 block_align = 0ULL;
    u64 data_size = 0ULL;

    if (out_info == (ush_wav_info *)0) {
        return 0;
    }

    if (ush_wav_read_exact(fd, riff_header, (u64)sizeof(riff_header)) == 0) {
        return 0;
    }

    if (ush_wav_tag_eq(&riff_header[0], "RIFF") == 0 || ush_wav_tag_eq(&riff_header[8], "WAVE") == 0) {
        return 0;
    }

    while (found_data == 0) {
        const unsigned char *chunk_tag;
        u64 chunk_size;

        if (ush_wav_read_exact(fd, chunk_header, (u64)sizeof(chunk_header)) == 0) {
            return 0;
        }

        chunk_tag = &chunk_header[0];
        chunk_size = (u64)ush_wav_le32(&chunk_header[4]);

        if (ush_wav_tag_eq(chunk_tag, "fmt ") != 0) {
            if (chunk_size < 16ULL) {
                return 0;
            }

            if (ush_wav_read_exact(fd, fmt_min, (u64)sizeof(fmt_min)) == 0) {
                return 0;
            }

            if (ush_wav_le16(&fmt_min[0]) != 1U) {
                return 0;
            }

            channels = (u64)ush_wav_le16(&fmt_min[2]);
            sample_rate = (u64)ush_wav_le32(&fmt_min[4]);
            block_align = (u64)ush_wav_le16(&fmt_min[12]);
            bits = (u64)ush_wav_le16(&fmt_min[14]);
            found_fmt = 1;

            if (chunk_size > 16ULL) {
                if (ush_wav_skip_bytes(fd, chunk_size - 16ULL) == 0) {
                    return 0;
                }
            }
        } else if (ush_wav_tag_eq(chunk_tag, "data") != 0) {
            data_size = chunk_size;
            found_data = 1;
        } else {
            if (ush_wav_skip_bytes(fd, chunk_size) == 0) {
                return 0;
            }
        }

        if ((chunk_size & 1ULL) != 0ULL && found_data == 0) {
            if (ush_wav_skip_bytes(fd, 1ULL) == 0) {
                return 0;
            }
        }
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

    out_info->data_size = data_size;
    out_info->sample_rate = sample_rate;
    out_info->channels = channels;
    out_info->bits_per_sample = bits;
    out_info->block_align = block_align;
    out_info->frame_count = data_size / block_align;

    return (out_info->frame_count > 0ULL) ? 1 : 0;
}

static u64 ush_wav_frame_deviation(const ush_wav_info *info, const unsigned char *frame) {
    if (info == (const ush_wav_info *)0 || frame == (const unsigned char *)0) {
        return 0ULL;
    }

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

static int ush_wavplay_parse_args(const char *arg, char *out_path, u64 out_path_size, u64 *out_steps, u64 *out_ticks,
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
    unsigned char frame_buf[8];
    ush_wav_info info;
    u64 fd;
    u64 steps;
    u64 ticks_per_step;
    u64 stride;
    u64 current_frame = 0ULL;
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

    fd = cleonos_sys_fd_open(abs_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        ush_writeln("wavplay: open failed");
        return 0;
    }

    if (ush_wav_parse_stream(fd, &info) == 0) {
        (void)cleonos_sys_fd_close(fd);
        ush_writeln("wavplay: unsupported wav (need PCM 8/16-bit, mono/stereo)");
        return 0;
    }

    if (info.block_align > (u64)sizeof(frame_buf)) {
        (void)cleonos_sys_fd_close(fd);
        ush_writeln("wavplay: unsupported block align");
        return 0;
    }

    if (steps > info.frame_count) {
        steps = info.frame_count;
    }

    if (steps == 0ULL) {
        (void)cleonos_sys_fd_close(fd);
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
        u64 skip_frames;

        if (frame_index >= info.frame_count) {
            frame_index = info.frame_count - 1ULL;
        }

        if (frame_index < current_frame) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln("wavplay: internal frame order error");
            return 0;
        }

        skip_frames = frame_index - current_frame;
        if (skip_frames > 0ULL) {
            if (skip_frames > (0xFFFFFFFFFFFFFFFFULL / info.block_align)) {
                (void)cleonos_sys_fd_close(fd);
                ush_writeln("wavplay: frame skip overflow");
                return 0;
            }

            if (ush_wav_skip_bytes(fd, skip_frames * info.block_align) == 0) {
                (void)cleonos_sys_fd_close(fd);
                ush_writeln("wavplay: seek/read failed");
                return 0;
            }
            current_frame = frame_index;
        }

        if (ush_wav_read_exact(fd, frame_buf, info.block_align) == 0) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln("wavplay: read failed");
            return 0;
        }
        current_frame++;

        deviation = ush_wav_frame_deviation(&info, frame_buf);

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
            (void)cleonos_sys_fd_close(fd);
            ush_writeln("wavplay: playback failed");
            (void)cleonos_sys_audio_stop();
            return 0;
        }

        run_freq = freq;
        run_ticks = ticks_per_step;
    }

    if (run_ticks > 0ULL) {
        if (cleonos_sys_audio_play_tone(run_freq, run_ticks) == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln("wavplay: playback failed");
            (void)cleonos_sys_audio_stop();
            return 0;
        }
    }

    (void)cleonos_sys_fd_close(fd);
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
