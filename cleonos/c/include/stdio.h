#ifndef CLEONOS_LIBC_STDIO_H
#define CLEONOS_LIBC_STDIO_H

#include <stdarg.h>

#ifndef EOF
#define EOF (-1)
#endif

int putchar(int ch);
int getchar(void);
int fputc(int ch, int fd);
int fgetc(int fd);
int fputs(const char *text, int fd);
int puts(const char *text);

int vsnprintf(char *out, unsigned long out_size, const char *fmt, va_list args);
int snprintf(char *out, unsigned long out_size, const char *fmt, ...);

int vdprintf(int fd, const char *fmt, va_list args);
int dprintf(int fd, const char *fmt, ...);
int vfprintf(int fd, const char *fmt, va_list args);
int fprintf(int fd, const char *fmt, ...);
int vprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...);

#endif
