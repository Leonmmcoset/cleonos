#include <stdio.h>

#include <cleonos_syscall.h>

typedef unsigned long clio_size_t;

#define CLIO_SINK_FD 1
#define CLIO_SINK_BUF 2

struct clio_sink {
    int mode;
    int fd;
    char *buffer;
    clio_size_t capacity;
    clio_size_t count;
    int failed;
};

static clio_size_t clio_strlen(const char *text) {
    clio_size_t len = 0UL;

    if (text == (const char *)0) {
        return 0UL;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static int clio_write_all_fd(int fd, const char *text, clio_size_t len) {
    const char *cursor = text;
    clio_size_t left = len;

    if (fd < 0 || (text == (const char *)0 && len != 0UL)) {
        return EOF;
    }

    while (left > 0UL) {
        u64 wrote = cleonos_sys_fd_write((u64)fd, cursor, (u64)left);
        clio_size_t progressed;

        if (wrote == 0ULL || wrote == (u64)-1) {
            return EOF;
        }

        progressed = (wrote > (u64)left) ? left : (clio_size_t)wrote;
        cursor += progressed;
        left -= progressed;
    }

    if (len > 0x7FFFFFFFUL) {
        return 0x7FFFFFFF;
    }

    return (int)len;
}

static void clio_sink_init_fd(struct clio_sink *sink, int fd) {
    sink->mode = CLIO_SINK_FD;
    sink->fd = fd;
    sink->buffer = (char *)0;
    sink->capacity = 0UL;
    sink->count = 0UL;
    sink->failed = 0;
}

static void clio_sink_init_buffer(struct clio_sink *sink, char *out, clio_size_t out_size) {
    sink->mode = CLIO_SINK_BUF;
    sink->fd = -1;
    sink->buffer = out;
    sink->capacity = out_size;
    sink->count = 0UL;
    sink->failed = 0;

    if (out != (char *)0 && out_size > 0UL) {
        out[0] = '\0';
    }
}

static void clio_sink_finalize_buffer(struct clio_sink *sink) {
    if (sink->mode != CLIO_SINK_BUF || sink->buffer == (char *)0 || sink->capacity == 0UL) {
        return;
    }

    if (sink->count < sink->capacity) {
        sink->buffer[sink->count] = '\0';
    } else {
        sink->buffer[sink->capacity - 1UL] = '\0';
    }
}

static int clio_sink_emit(struct clio_sink *sink, const char *text, clio_size_t len) {
    if (sink == (struct clio_sink *)0 || text == (const char *)0 || len == 0UL) {
        return 1;
    }

    if (sink->mode == CLIO_SINK_FD) {
        if (clio_write_all_fd(sink->fd, text, len) == EOF) {
            sink->failed = 1;
            return 0;
        }
    } else {
        if (sink->buffer != (char *)0 && sink->capacity > 0UL) {
            clio_size_t i;
            clio_size_t start = sink->count;
            clio_size_t writable = 0UL;

            if (start + 1UL < sink->capacity) {
                writable = (sink->capacity - 1UL) - start;
            }

            if (len < writable) {
                writable = len;
            }

            for (i = 0UL; i < writable; i++) {
                sink->buffer[start + i] = text[i];
            }
        }
    }

    sink->count += len;
    return 1;
}

static clio_size_t clio_u64_to_base(unsigned long long value, unsigned int base, int uppercase, char *out,
                                    clio_size_t out_size) {
    char tmp[64];
    clio_size_t pos = 0UL;
    clio_size_t i = 0UL;

    if (out == (char *)0 || out_size < 2UL || base < 2U || base > 16U) {
        return 0UL;
    }

    if (value == 0ULL) {
        out[0] = '0';
        out[1] = '\0';
        return 1UL;
    }

    while (value != 0ULL && pos < (clio_size_t)sizeof(tmp)) {
        unsigned long long digit = value % (unsigned long long)base;

        if (digit < 10ULL) {
            tmp[pos++] = (char)('0' + digit);
        } else {
            char alpha = (uppercase != 0) ? 'A' : 'a';
            tmp[pos++] = (char)(alpha + (char)(digit - 10ULL));
        }

        value /= (unsigned long long)base;
    }

    if (pos + 1UL > out_size) {
        return 0UL;
    }

    while (pos > 0UL) {
        pos--;
        out[i++] = tmp[pos];
    }

    out[i] = '\0';
    return i;
}

static clio_size_t clio_i64_to_dec(long long value, char *out, clio_size_t out_size) {
    unsigned long long magnitude;
    clio_size_t offset = 0UL;
    clio_size_t len;

    if (out == (char *)0 || out_size < 2UL) {
        return 0UL;
    }

    if (value < 0LL) {
        out[offset++] = '-';
        if (offset + 2UL > out_size) {
            return 0UL;
        }
        magnitude = (unsigned long long)(-(value + 1LL)) + 1ULL;
    } else {
        magnitude = (unsigned long long)value;
    }

    len = clio_u64_to_base(magnitude, 10U, 0, out + offset, out_size - offset);

    if (len == 0UL) {
        return 0UL;
    }

    return offset + len;
}

static int clio_vformat(struct clio_sink *sink, const char *fmt, va_list args) {
    const char *cursor = fmt;

    if (sink == (struct clio_sink *)0 || fmt == (const char *)0) {
        return EOF;
    }

    while (*cursor != '\0') {
        if (*cursor != '%') {
            if (clio_sink_emit(sink, cursor, 1UL) == 0) {
                return EOF;
            }
            cursor++;
            continue;
        }

        cursor++;

        if (*cursor == '%') {
            if (clio_sink_emit(sink, "%", 1UL) == 0) {
                return EOF;
            }
            cursor++;
            continue;
        }

        {
            int length_mode = 0; /* 0: default, 1: l, 2: ll, 3: z */
            char spec = *cursor;
            char number_buf[64];
            clio_size_t out_len = 0UL;

            if (spec == 'l') {
                cursor++;
                spec = *cursor;
                length_mode = 1;

                if (spec == 'l') {
                    cursor++;
                    spec = *cursor;
                    length_mode = 2;
                }
            } else if (spec == 'z') {
                cursor++;
                spec = *cursor;
                length_mode = 3;
            }

            if (spec == '\0') {
                break;
            }

            if (spec == 's') {
                const char *text = va_arg(args, const char *);

                if (text == (const char *)0) {
                    text = "(null)";
                }

                out_len = clio_strlen(text);

                if (clio_sink_emit(sink, text, out_len) == 0) {
                    return EOF;
                }
            } else if (spec == 'c') {
                char out = (char)va_arg(args, int);

                if (clio_sink_emit(sink, &out, 1UL) == 0) {
                    return EOF;
                }
            } else if (spec == 'd' || spec == 'i') {
                long long value;

                if (length_mode == 2) {
                    value = va_arg(args, long long);
                } else if (length_mode == 1) {
                    value = (long long)va_arg(args, long);
                } else if (length_mode == 3) {
                    value = (long long)va_arg(args, long long);
                } else {
                    value = (long long)va_arg(args, int);
                }

                out_len = clio_i64_to_dec(value, number_buf, (clio_size_t)sizeof(number_buf));
                if (out_len == 0UL || clio_sink_emit(sink, number_buf, out_len) == 0) {
                    return EOF;
                }
            } else if (spec == 'u' || spec == 'x' || spec == 'X') {
                unsigned long long value;
                unsigned int base = (spec == 'u') ? 10U : 16U;
                int upper = (spec == 'X') ? 1 : 0;

                if (length_mode == 2) {
                    value = va_arg(args, unsigned long long);
                } else if (length_mode == 1) {
                    value = (unsigned long long)va_arg(args, unsigned long);
                } else if (length_mode == 3) {
                    value = (unsigned long long)va_arg(args, unsigned long long);
                } else {
                    value = (unsigned long long)va_arg(args, unsigned int);
                }

                out_len = clio_u64_to_base(value, base, upper, number_buf, (clio_size_t)sizeof(number_buf));
                if (out_len == 0UL || clio_sink_emit(sink, number_buf, out_len) == 0) {
                    return EOF;
                }
            } else if (spec == 'p') {
                const void *ptr = va_arg(args, const void *);
                unsigned long long value = (unsigned long long)(unsigned long)ptr;

                if (clio_sink_emit(sink, "0x", 2UL) == 0) {
                    return EOF;
                }

                out_len = clio_u64_to_base(value, 16U, 0, number_buf, (clio_size_t)sizeof(number_buf));
                if (out_len == 0UL || clio_sink_emit(sink, number_buf, out_len) == 0) {
                    return EOF;
                }
            } else {
                char fallback[2];
                fallback[0] = spec;
                fallback[1] = '\0';

                if (clio_sink_emit(sink, "%", 1UL) == 0 || clio_sink_emit(sink, fallback, 1UL) == 0) {
                    return EOF;
                }
            }

            cursor++;
        }
    }

    if (sink->mode == CLIO_SINK_BUF) {
        clio_sink_finalize_buffer(sink);
    }

    if (sink->failed != 0) {
        return EOF;
    }

    if (sink->count > 0x7FFFFFFFUL) {
        return 0x7FFFFFFF;
    }

    return (int)sink->count;
}

int putchar(int ch) {
    char out = (char)(ch & 0xFF);
    return (clio_write_all_fd(1, &out, 1UL) == EOF) ? EOF : (int)(unsigned char)out;
}

int getchar(void) {
    char ch = '\0';

    for (;;) {
        u64 got = cleonos_sys_fd_read(0ULL, &ch, 1ULL);

        if (got == 1ULL) {
            return (int)(unsigned char)ch;
        }

        if (got == (u64)-1) {
            return EOF;
        }

        (void)cleonos_sys_yield();
    }
}

int fputc(int ch, int fd) {
    char out = (char)(ch & 0xFF);
    return (clio_write_all_fd(fd, &out, 1UL) == EOF) ? EOF : (int)(unsigned char)out;
}

int fgetc(int fd) {
    char ch = '\0';

    if (fd < 0) {
        return EOF;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read((u64)fd, &ch, 1ULL);

        if (got == 1ULL) {
            return (int)(unsigned char)ch;
        }

        if (got == (u64)-1) {
            return EOF;
        }

        (void)cleonos_sys_yield();
    }
}

int fputs(const char *text, int fd) {
    clio_size_t len;

    if (text == (const char *)0) {
        return EOF;
    }

    len = clio_strlen(text);
    return clio_write_all_fd(fd, text, len);
}

int puts(const char *text) {
    int wrote = fputs(text, 1);

    if (wrote == EOF) {
        return EOF;
    }

    if (putchar('\n') == EOF) {
        return EOF;
    }

    return wrote + 1;
}

int vsnprintf(char *out, unsigned long out_size, const char *fmt, va_list args) {
    struct clio_sink sink;
    va_list args_copy;
    int rc;

    clio_sink_init_buffer(&sink, out, out_size);
    va_copy(args_copy, args);
    rc = clio_vformat(&sink, fmt, args_copy);
    va_end(args_copy);

    return rc;
}

int snprintf(char *out, unsigned long out_size, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vsnprintf(out, out_size, fmt, args);
    va_end(args);

    return rc;
}

int vdprintf(int fd, const char *fmt, va_list args) {
    struct clio_sink sink;
    va_list args_copy;
    int rc;

    clio_sink_init_fd(&sink, fd);
    va_copy(args_copy, args);
    rc = clio_vformat(&sink, fmt, args_copy);
    va_end(args_copy);

    return rc;
}

int dprintf(int fd, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vdprintf(fd, fmt, args);
    va_end(args);

    return rc;
}

int vfprintf(int fd, const char *fmt, va_list args) {
    return vdprintf(fd, fmt, args);
}

int fprintf(int fd, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vdprintf(fd, fmt, args);
    va_end(args);

    return rc;
}

int vprintf(const char *fmt, va_list args) {
    return vdprintf(1, fmt, args);
}

int printf(const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vdprintf(1, fmt, args);
    va_end(args);

    return rc;
}
