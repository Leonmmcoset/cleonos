#include <stdlib.h>

#include <ctype.h>
#include <limits.h>

#include <cleonos_syscall.h>

static int clib_digit_value(int ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'z') {
        return 10 + (ch - 'a');
    }

    if (ch >= 'A' && ch <= 'Z') {
        return 10 + (ch - 'A');
    }

    return -1;
}

static const char *clib_skip_space(const char *text) {
    const char *p = text;

    if (p == (const char *)0) {
        return (const char *)0;
    }

    while (*p != '\0' && isspace((unsigned char)*p) != 0) {
        p++;
    }

    return p;
}

int abs(int value) {
    return (value < 0) ? -value : value;
}

long labs(long value) {
    return (value < 0L) ? -value : value;
}

long long llabs(long long value) {
    return (value < 0LL) ? -value : value;
}

int atoi(const char *text) {
    return (int)strtol(text, (char **)0, 10);
}

long atol(const char *text) {
    return strtol(text, (char **)0, 10);
}

long long atoll(const char *text) {
    return strtoll(text, (char **)0, 10);
}

unsigned long strtoul(const char *text, char **out_end, int base) {
    const char *p = clib_skip_space(text);
    int negative = 0;
    unsigned long value = 0UL;
    int any = 0;
    int overflow = 0;

    if (out_end != (char **)0) {
        *out_end = (char *)text;
    }

    if (p == (const char *)0) {
        return 0UL;
    }

    if (*p == '+' || *p == '-') {
        negative = (*p == '-') ? 1 : 0;
        p++;
    }

    if (base == 0) {
        if (p[0] == '0') {
            if ((p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2]) != 0) {
                base = 16;
                p += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }

    if (base < 2 || base > 36) {
        return 0UL;
    }

    while (*p != '\0') {
        int digit = clib_digit_value((unsigned char)*p);

        if (digit < 0 || digit >= base) {
            break;
        }

        any = 1;

        if (value > (ULONG_MAX - (unsigned long)digit) / (unsigned long)base) {
            overflow = 1;
            value = ULONG_MAX;
        } else if (overflow == 0) {
            value = value * (unsigned long)base + (unsigned long)digit;
        }

        p++;
    }

    if (any == 0) {
        return 0UL;
    }

    if (out_end != (char **)0) {
        *out_end = (char *)p;
    }

    if (negative != 0) {
        return (unsigned long)(0UL - value);
    }

    return value;
}

long strtol(const char *text, char **out_end, int base) {
    const char *p = clib_skip_space(text);
    int negative = 0;
    unsigned long long value = 0ULL;
    unsigned long long limit;
    int any = 0;
    int overflow = 0;

    if (out_end != (char **)0) {
        *out_end = (char *)text;
    }

    if (p == (const char *)0) {
        return 0L;
    }

    if (*p == '+' || *p == '-') {
        negative = (*p == '-') ? 1 : 0;
        p++;
    }

    if (base == 0) {
        if (p[0] == '0') {
            if ((p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2]) != 0) {
                base = 16;
                p += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }

    if (base < 2 || base > 36) {
        return 0L;
    }

    limit = (negative != 0) ? ((unsigned long long)LONG_MAX + 1ULL) : (unsigned long long)LONG_MAX;

    while (*p != '\0') {
        int digit = clib_digit_value((unsigned char)*p);

        if (digit < 0 || digit >= base) {
            break;
        }

        any = 1;

        if (value > (limit - (unsigned long long)digit) / (unsigned long long)base) {
            overflow = 1;
            value = limit;
        } else if (overflow == 0) {
            value = value * (unsigned long long)base + (unsigned long long)digit;
        }

        p++;
    }

    if (any == 0) {
        return 0L;
    }

    if (out_end != (char **)0) {
        *out_end = (char *)p;
    }

    if (overflow != 0) {
        return (negative != 0) ? LONG_MIN : LONG_MAX;
    }

    if (negative != 0) {
        if (value == ((unsigned long long)LONG_MAX + 1ULL)) {
            return LONG_MIN;
        }
        return -(long)value;
    }

    return (long)value;
}

long long strtoll(const char *text, char **out_end, int base) {
    return (long long)strtol(text, out_end, base);
}

unsigned long long strtoull(const char *text, char **out_end, int base) {
    return (unsigned long long)strtoul(text, out_end, base);
}

static unsigned long clib_rand_state = 1UL;

void srand(unsigned int seed) {
    clib_rand_state = (unsigned long)seed;
    if (clib_rand_state == 0UL) {
        clib_rand_state = 1UL;
    }
}

int rand(void) {
    clib_rand_state = (1103515245UL * clib_rand_state) + 12345UL;
    return (int)((clib_rand_state >> 16) & (unsigned long)RAND_MAX);
}

void exit(int status) {
    (void)cleonos_sys_exit((u64)(unsigned long long)status);

    for (;;) {
        (void)cleonos_sys_yield();
    }
}

void abort(void) {
    exit(EXIT_FAILURE);
}
