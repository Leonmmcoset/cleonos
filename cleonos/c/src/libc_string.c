#include <string.h>

void *memset(void *dst, int value, size_t size) {
    unsigned char *d = (unsigned char *)dst;
    unsigned char byte = (unsigned char)value;
    size_t i;

    if (d == (unsigned char *)0) {
        return dst;
    }

    for (i = 0U; i < size; i++) {
        d[i] = byte;
    }

    return dst;
}

void *memcpy(void *dst, const void *src, size_t size) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    size_t i;

    if (d == (unsigned char *)0 || s == (const unsigned char *)0) {
        return dst;
    }

    for (i = 0U; i < size; i++) {
        d[i] = s[i];
    }

    return dst;
}

void *memmove(void *dst, const void *src, size_t size) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    size_t i;

    if (d == (unsigned char *)0 || s == (const unsigned char *)0 || d == s || size == 0U) {
        return dst;
    }

    if (d < s || d >= s + size) {
        for (i = 0U; i < size; i++) {
            d[i] = s[i];
        }
    } else {
        for (i = size; i > 0U; i--) {
            d[i - 1U] = s[i - 1U];
        }
    }

    return dst;
}

int memcmp(const void *left, const void *right, size_t size) {
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    size_t i;

    if (a == b || size == 0U) {
        return 0;
    }

    if (a == (const unsigned char *)0) {
        return -1;
    }

    if (b == (const unsigned char *)0) {
        return 1;
    }

    for (i = 0U; i < size; i++) {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
    }

    return 0;
}

void *memchr(const void *src, int value, size_t size) {
    const unsigned char *s = (const unsigned char *)src;
    unsigned char needle = (unsigned char)value;
    size_t i;

    if (s == (const unsigned char *)0) {
        return (void *)0;
    }

    for (i = 0U; i < size; i++) {
        if (s[i] == needle) {
            return (void *)(s + i);
        }
    }

    return (void *)0;
}

size_t strlen(const char *text) {
    size_t len = 0U;

    if (text == (const char *)0) {
        return 0U;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

size_t strnlen(const char *text, size_t max_size) {
    size_t len = 0U;

    if (text == (const char *)0) {
        return 0U;
    }

    while (len < max_size && text[len] != '\0') {
        len++;
    }

    return len;
}

char *strcpy(char *dst, const char *src) {
    size_t i = 0U;

    if (dst == (char *)0 || src == (const char *)0) {
        return dst;
    }

    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
    return dst;
}

char *strncpy(char *dst, const char *src, size_t size) {
    size_t i = 0U;

    if (dst == (char *)0 || src == (const char *)0) {
        return dst;
    }

    while (i < size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    while (i < size) {
        dst[i++] = '\0';
    }

    return dst;
}

int strcmp(const char *left, const char *right) {
    size_t i = 0U;

    if (left == right) {
        return 0;
    }

    if (left == (const char *)0) {
        return -1;
    }

    if (right == (const char *)0) {
        return 1;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return (left[i] < right[i]) ? -1 : 1;
        }
        i++;
    }

    if (left[i] == right[i]) {
        return 0;
    }

    return (left[i] < right[i]) ? -1 : 1;
}

int strncmp(const char *left, const char *right, size_t size) {
    size_t i = 0U;

    if (size == 0U || left == right) {
        return 0;
    }

    if (left == (const char *)0) {
        return -1;
    }

    if (right == (const char *)0) {
        return 1;
    }

    while (i < size && left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return (left[i] < right[i]) ? -1 : 1;
        }
        i++;
    }

    if (i == size) {
        return 0;
    }

    if (left[i] == right[i]) {
        return 0;
    }

    return (left[i] < right[i]) ? -1 : 1;
}

char *strchr(const char *text, int ch) {
    char needle = (char)ch;
    size_t i = 0U;

    if (text == (const char *)0) {
        return (char *)0;
    }

    while (text[i] != '\0') {
        if (text[i] == needle) {
            return (char *)(text + i);
        }
        i++;
    }

    if (needle == '\0') {
        return (char *)(text + i);
    }

    return (char *)0;
}

char *strrchr(const char *text, int ch) {
    char needle = (char)ch;
    const char *last = (const char *)0;
    size_t i = 0U;

    if (text == (const char *)0) {
        return (char *)0;
    }

    while (text[i] != '\0') {
        if (text[i] == needle) {
            last = text + i;
        }
        i++;
    }

    if (needle == '\0') {
        return (char *)(text + i);
    }

    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    size_t i;
    size_t j;
    size_t nlen;

    if (haystack == (const char *)0 || needle == (const char *)0) {
        return (char *)0;
    }

    nlen = strlen(needle);
    if (nlen == 0U) {
        return (char *)haystack;
    }

    for (i = 0U; haystack[i] != '\0'; i++) {
        if (haystack[i] != needle[0]) {
            continue;
        }

        for (j = 1U; j < nlen; j++) {
            if (haystack[i + j] == '\0' || haystack[i + j] != needle[j]) {
                break;
            }
        }

        if (j == nlen) {
            return (char *)(haystack + i);
        }
    }

    return (char *)0;
}

static int clib_delim_contains(const char *delim, char ch) {
    size_t i = 0U;

    if (delim == (const char *)0) {
        return 0;
    }

    while (delim[i] != '\0') {
        if (delim[i] == ch) {
            return 1;
        }
        i++;
    }

    return 0;
}

size_t strspn(const char *text, const char *accept) {
    size_t n = 0U;

    if (text == (const char *)0 || accept == (const char *)0) {
        return 0U;
    }

    while (text[n] != '\0' && clib_delim_contains(accept, text[n]) != 0) {
        n++;
    }

    return n;
}

size_t strcspn(const char *text, const char *reject) {
    size_t n = 0U;

    if (text == (const char *)0 || reject == (const char *)0) {
        return 0U;
    }

    while (text[n] != '\0' && clib_delim_contains(reject, text[n]) == 0) {
        n++;
    }

    return n;
}

char *strpbrk(const char *text, const char *accept) {
    size_t i = 0U;

    if (text == (const char *)0 || accept == (const char *)0) {
        return (char *)0;
    }

    while (text[i] != '\0') {
        if (clib_delim_contains(accept, text[i]) != 0) {
            return (char *)(text + i);
        }
        i++;
    }

    return (char *)0;
}

char *strtok_r(char *text, const char *delim, char **saveptr) {
    char *start;
    char *cursor;

    if (delim == (const char *)0 || saveptr == (char **)0) {
        return (char *)0;
    }

    if (text != (char *)0) {
        cursor = text;
    } else {
        cursor = *saveptr;
    }

    if (cursor == (char *)0) {
        return (char *)0;
    }

    while (*cursor != '\0' && clib_delim_contains(delim, *cursor) != 0) {
        cursor++;
    }

    if (*cursor == '\0') {
        *saveptr = cursor;
        return (char *)0;
    }

    start = cursor;
    while (*cursor != '\0' && clib_delim_contains(delim, *cursor) == 0) {
        cursor++;
    }

    if (*cursor == '\0') {
        *saveptr = cursor;
    } else {
        *cursor = '\0';
        *saveptr = cursor + 1;
    }

    return start;
}

char *strtok(char *text, const char *delim) {
    static char *state = (char *)0;
    return strtok_r(text, delim, &state);
}

char *strcat(char *dst, const char *src) {
    size_t dlen = strlen(dst);
    size_t i = 0U;

    if (dst == (char *)0 || src == (const char *)0) {
        return dst;
    }

    while (src[i] != '\0') {
        dst[dlen + i] = src[i];
        i++;
    }

    dst[dlen + i] = '\0';
    return dst;
}

char *strncat(char *dst, const char *src, size_t size) {
    size_t dlen = strlen(dst);
    size_t i = 0U;

    if (dst == (char *)0 || src == (const char *)0) {
        return dst;
    }

    while (i < size && src[i] != '\0') {
        dst[dlen + i] = src[i];
        i++;
    }

    dst[dlen + i] = '\0';
    return dst;
}
