#include <clks/string.h>
#include <clks/types.h>

void *memcpy(void *dst, const void *src, usize count) {
    return clks_memcpy(dst, src, count);
}

void *memmove(void *dst, const void *src, usize count) {
    return clks_memmove(dst, src, count);
}

void *memset(void *dst, int value, usize count) {
    return clks_memset(dst, value, count);
}

int memcmp(const void *left, const void *right, usize count) {
    const u8 *a = (const u8 *)left;
    const u8 *b = (const u8 *)right;
    usize i;

    for (i = 0U; i < count; i++) {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
    }

    return 0;
}

int bcmp(const void *left, const void *right, usize count) {
    return memcmp(left, right, count);
}