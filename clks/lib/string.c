#include <clks/string.h>

usize clks_strlen(const char *str) {
    usize len = 0;

    while (str[len] != '\0') {
        len++;
    }

    return len;
}

void *clks_memset(void *dst, int value, usize count) {
    u8 *d = (u8 *)dst;
    u8 v = (u8)value;
    usize i;

    for (i = 0; i < count; i++) {
        d[i] = v;
    }

    return dst;
}

void *clks_memcpy(void *dst, const void *src, usize count) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    usize i;

    for (i = 0; i < count; i++) {
        d[i] = s[i];
    }

    return dst;
}

void *clks_memmove(void *dst, const void *src, usize count) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    usize i;

    if (d == s || count == 0) {
        return dst;
    }

    if (d < s) {
        for (i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        for (i = count; i != 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dst;
}

int clks_strcmp(const char *left, const char *right) {
    usize i = 0;

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return (int)((u8)left[i] - (u8)right[i]);
        }
        i++;
    }

    return (int)((u8)left[i] - (u8)right[i]);
}