#ifndef CLKS_STRING_H
#define CLKS_STRING_H

#include <clks/types.h>

usize clks_strlen(const char *str);
void *clks_memset(void *dst, int value, usize count);
void *clks_memcpy(void *dst, const void *src, usize count);
void *clks_memmove(void *dst, const void *src, usize count);
int clks_strcmp(const char *left, const char *right);

#endif