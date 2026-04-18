#ifndef CLEONOS_LIBC_STDLIB_H
#define CLEONOS_LIBC_STDLIB_H

#include <stddef.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

int abs(int value);
long labs(long value);
long long llabs(long long value);

int atoi(const char *text);
long atol(const char *text);
long long atoll(const char *text);
long strtol(const char *text, char **out_end, int base);
unsigned long strtoul(const char *text, char **out_end, int base);
long long strtoll(const char *text, char **out_end, int base);
unsigned long long strtoull(const char *text, char **out_end, int base);

void srand(unsigned int seed);
int rand(void);

void exit(int status);
void abort(void);

#endif
