#ifndef CLEONOS_LIBC_STRING_H
#define CLEONOS_LIBC_STRING_H

#include <stddef.h>

void *memset(void *dst, int value, size_t size);
void *memcpy(void *dst, const void *src, size_t size);
void *memmove(void *dst, const void *src, size_t size);
int memcmp(const void *left, const void *right, size_t size);
void *memchr(const void *src, int value, size_t size);

size_t strlen(const char *text);
size_t strnlen(const char *text, size_t max_size);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t size);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t size);
char *strchr(const char *text, int ch);
char *strrchr(const char *text, int ch);
char *strstr(const char *haystack, const char *needle);
size_t strspn(const char *text, const char *accept);
size_t strcspn(const char *text, const char *reject);
char *strpbrk(const char *text, const char *accept);
char *strtok_r(char *text, const char *delim, char **saveptr);
char *strtok(char *text, const char *delim);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t size);

#endif
