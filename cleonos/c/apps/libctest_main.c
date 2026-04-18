#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_total = 0;
static int g_failed = 0;

static int test_vsnprintf(char *out, unsigned long out_size, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vsnprintf(out, out_size, fmt, args);
    va_end(args);
    return rc;
}

static void test_fail(const char *group, int line, const char *expr) {
    g_failed++;
    (void)printf("[libctest][FAIL] %s:%d %s\n", group, line, expr);
}

#define TEST_ASSERT(group, expr)            \
    do {                                    \
        g_total++;                          \
        if (!(expr)) {                      \
            test_fail((group), __LINE__, #expr); \
        }                                   \
    } while (0)

static void test_string_lib(void) {
    char buf[32];
    char src[16];
    char tok_text[32];
    char tok_text2[32];
    char *save = (char *)0;
    char *tok;
    void *ptr;

    (void)memset(buf, 0xAA, sizeof(buf));
    ptr = memset(buf, 'x', 5U);
    TEST_ASSERT("string", ptr == buf);
    TEST_ASSERT("string", buf[0] == 'x' && buf[4] == 'x');
    TEST_ASSERT("string", memset((void *)0, 1, 4U) == (void *)0);

    (void)memset(buf, 0, sizeof(buf));
    (void)strcpy(src, "abcdef");
    ptr = memcpy(buf, src, 6U);
    TEST_ASSERT("string", ptr == buf);
    TEST_ASSERT("string", memcmp(buf, "abcdef", 6U) == 0);
    TEST_ASSERT("string", memcpy((void *)0, src, 3U) == (void *)0);

    (void)strcpy(buf, "abcdef");
    (void)memmove(buf + 2, buf, 4U);
    TEST_ASSERT("string", memcmp(buf, "ababcd", 6U) == 0);
    (void)strcpy(buf, "abcdef");
    (void)memmove(buf, buf + 2, 4U);
    TEST_ASSERT("string", memcmp(buf, "cdef", 4U) == 0);
    TEST_ASSERT("string", memmove((void *)0, src, 3U) == (void *)0);

    TEST_ASSERT("string", memcmp("abc", "abc", 3U) == 0);
    TEST_ASSERT("string", memcmp("abc", "abd", 3U) < 0);
    TEST_ASSERT("string", memcmp("abe", "abd", 3U) > 0);
    TEST_ASSERT("string", memcmp((const void *)0, "a", 1U) < 0);
    TEST_ASSERT("string", memcmp("a", (const void *)0, 1U) > 0);

    TEST_ASSERT("string", memchr("abc", 'b', 3U) == (const void *)("abc" + 1));
    TEST_ASSERT("string", memchr("abc", 'z', 3U) == (void *)0);
    TEST_ASSERT("string", memchr((const void *)0, 'a', 1U) == (void *)0);

    TEST_ASSERT("string", strlen("abc") == 3U);
    TEST_ASSERT("string", strlen("") == 0U);
    TEST_ASSERT("string", strlen((const char *)0) == 0U);
    TEST_ASSERT("string", strnlen("abcdef", 3U) == 3U);
    TEST_ASSERT("string", strnlen("ab", 8U) == 2U);

    (void)strcpy(buf, "hello");
    TEST_ASSERT("string", strcmp(buf, "hello") == 0);
    TEST_ASSERT("string", strcpy((char *)0, "x") == (char *)0);
    TEST_ASSERT("string", strcmp((const char *)0, "x") < 0);
    TEST_ASSERT("string", strcmp("x", (const char *)0) > 0);
    TEST_ASSERT("string", strcmp((const char *)0, (const char *)0) == 0);

    (void)memset(buf, 'Z', 8U);
    (void)strncpy(buf, "hi", 6U);
    TEST_ASSERT("string", buf[0] == 'h' && buf[1] == 'i');
    TEST_ASSERT("string", buf[2] == '\0' && buf[5] == '\0');
    (void)strncpy(buf, "abcdef", 3U);
    TEST_ASSERT("string", buf[0] == 'a' && buf[1] == 'b' && buf[2] == 'c');
    TEST_ASSERT("string", strncmp("abc", "abd", 2U) == 0);
    TEST_ASSERT("string", strncmp("abc", "abd", 3U) < 0);
    TEST_ASSERT("string", strncmp((const char *)0, "a", 1U) < 0);

    TEST_ASSERT("string", strchr("abc", 'b') == (char *)("abc" + 1));
    TEST_ASSERT("string", strchr("abc", '\0') == (char *)("abc" + 3));
    TEST_ASSERT("string", strchr((const char *)0, 'a') == (char *)0);
    TEST_ASSERT("string", strrchr("abca", 'a') == (char *)("abca" + 3));
    TEST_ASSERT("string", strrchr("abc", 'z') == (char *)0);

    TEST_ASSERT("string", strstr("hello world", "world") == (char *)("hello world" + 6));
    TEST_ASSERT("string", strstr("abc", "") == (char *)"abc");
    TEST_ASSERT("string", strstr((const char *)0, "a") == (char *)0);
    TEST_ASSERT("string", strspn("abc123", "abc") == 3U);
    TEST_ASSERT("string", strspn((const char *)0, "abc") == 0U);
    TEST_ASSERT("string", strcspn("abc123", "123") == 3U);
    TEST_ASSERT("string", strcspn("abcdef", (const char *)0) == 0U);
    TEST_ASSERT("string", strpbrk("abcdef", "xzde") == (char *)("abcdef" + 3));
    TEST_ASSERT("string", strpbrk("abcdef", "XYZ") == (char *)0);

    (void)strcpy(buf, "ab");
    (void)strcat(buf, "cd");
    TEST_ASSERT("string", strcmp(buf, "abcd") == 0);
    (void)strncat(buf, "efgh", 2U);
    TEST_ASSERT("string", strcmp(buf, "abcdef") == 0);
    TEST_ASSERT("string", strcat((char *)0, "x") == (char *)0);
    TEST_ASSERT("string", strncat((char *)0, "x", 1U) == (char *)0);

    (void)strcpy(tok_text, "a,,b;c");
    tok = strtok_r(tok_text, ",;", &save);
    TEST_ASSERT("string", tok != (char *)0 && strcmp(tok, "a") == 0);
    tok = strtok_r((char *)0, ",;", &save);
    TEST_ASSERT("string", tok != (char *)0 && strcmp(tok, "b") == 0);
    tok = strtok_r((char *)0, ",;", &save);
    TEST_ASSERT("string", tok != (char *)0 && strcmp(tok, "c") == 0);
    tok = strtok_r((char *)0, ",;", &save);
    TEST_ASSERT("string", tok == (char *)0);
    TEST_ASSERT("string", strtok_r(tok_text, (const char *)0, &save) == (char *)0);
    TEST_ASSERT("string", strtok_r(tok_text, ",", (char **)0) == (char *)0);

    (void)strcpy(tok_text2, "x y");
    tok = strtok(tok_text2, " ");
    TEST_ASSERT("string", tok != (char *)0 && strcmp(tok, "x") == 0);
    tok = strtok((char *)0, " ");
    TEST_ASSERT("string", tok != (char *)0 && strcmp(tok, "y") == 0);
    tok = strtok((char *)0, " ");
    TEST_ASSERT("string", tok == (char *)0);
}

static void test_ctype_lib(void) {
    TEST_ASSERT("ctype", isspace(' ') != 0);
    TEST_ASSERT("ctype", isspace('\n') != 0);
    TEST_ASSERT("ctype", isspace('A') == 0);
    TEST_ASSERT("ctype", isdigit('0') != 0 && isdigit('9') != 0);
    TEST_ASSERT("ctype", isdigit('x') == 0);
    TEST_ASSERT("ctype", isalpha('a') != 0 && isalpha('Z') != 0);
    TEST_ASSERT("ctype", isalpha('1') == 0);
    TEST_ASSERT("ctype", isalnum('b') != 0 && isalnum('8') != 0);
    TEST_ASSERT("ctype", isalnum('#') == 0);
    TEST_ASSERT("ctype", isxdigit('f') != 0 && isxdigit('A') != 0 && isxdigit('9') != 0);
    TEST_ASSERT("ctype", isxdigit('g') == 0);
    TEST_ASSERT("ctype", isupper('Q') != 0 && isupper('q') == 0);
    TEST_ASSERT("ctype", islower('q') != 0 && islower('Q') == 0);
    TEST_ASSERT("ctype", isprint(' ') != 0 && isprint('~') != 0);
    TEST_ASSERT("ctype", isprint('\x1F') == 0);
    TEST_ASSERT("ctype", iscntrl('\n') != 0 && iscntrl(0x7F) != 0);
    TEST_ASSERT("ctype", iscntrl('A') == 0);
    TEST_ASSERT("ctype", tolower('A') == 'a');
    TEST_ASSERT("ctype", tolower('z') == 'z');
    TEST_ASSERT("ctype", toupper('a') == 'A');
    TEST_ASSERT("ctype", toupper('Z') == 'Z');
}

static void test_stdlib_lib(void) {
    char *end = (char *)0;
    static const char no_digits[] = "xyz";
    int r1;
    int r2;

    TEST_ASSERT("stdlib", abs(-7) == 7 && abs(5) == 5);
    TEST_ASSERT("stdlib", labs(-9L) == 9L && labs(6L) == 6L);
    TEST_ASSERT("stdlib", llabs(-11LL) == 11LL && llabs(4LL) == 4LL);
    TEST_ASSERT("stdlib", atoi("123") == 123);
    TEST_ASSERT("stdlib", atol("-456") == -456L);
    TEST_ASSERT("stdlib", atoll("789") == 789LL);

    TEST_ASSERT("stdlib", strtol("  -42", &end, 10) == -42L && *end == '\0');
    TEST_ASSERT("stdlib", strtol("0x10", &end, 0) == 16L && *end == '\0');
    TEST_ASSERT("stdlib", strtol("077", &end, 0) == 63L && *end == '\0');
    TEST_ASSERT("stdlib", strtol(no_digits, &end, 10) == 0L && end == no_digits);
    TEST_ASSERT("stdlib", strtol("9999999999999999999999999", &end, 10) == LONG_MAX);
    TEST_ASSERT("stdlib", strtol("-9999999999999999999999999", &end, 10) == LONG_MIN);

    TEST_ASSERT("stdlib", strtoul("42", &end, 10) == 42UL && *end == '\0');
    TEST_ASSERT("stdlib", strtoul("0x20", &end, 0) == 32UL && *end == '\0');
    TEST_ASSERT("stdlib", strtoul("-1", &end, 10) == ULONG_MAX);
    TEST_ASSERT("stdlib", strtoull("1234", &end, 10) == 1234ULL && *end == '\0');
    TEST_ASSERT("stdlib", strtoll("-1234", &end, 10) == -1234LL && *end == '\0');

    srand(12345U);
    r1 = rand();
    r2 = rand();
    TEST_ASSERT("stdlib", r1 >= 0 && r1 <= RAND_MAX);
    TEST_ASSERT("stdlib", r2 >= 0 && r2 <= RAND_MAX);
    srand(12345U);
    TEST_ASSERT("stdlib", rand() == r1);
    TEST_ASSERT("stdlib", rand() == r2);
}

static void test_stdio_lib(void) {
    char buf[64];
    int rc;
    int c;

    rc = snprintf(buf, sizeof(buf), "A:%d B:%u C:%X %s", -7, 9U, 0x2AU, "ok");
    TEST_ASSERT("stdio", rc > 0);
    TEST_ASSERT("stdio", strcmp(buf, "A:-7 B:9 C:2A ok") == 0);

    rc = snprintf(buf, 5UL, "abcdef");
    TEST_ASSERT("stdio", rc == 6);
    TEST_ASSERT("stdio", strcmp(buf, "abcd") == 0);

    rc = snprintf(buf, sizeof(buf), "ptr=%p", (void *)buf);
    TEST_ASSERT("stdio", rc > 4);
    TEST_ASSERT("stdio", strstr(buf, "ptr=0x") == buf);

    rc = snprintf(buf, sizeof(buf), "%s", (const char *)0);
    TEST_ASSERT("stdio", rc == 6);
    TEST_ASSERT("stdio", strcmp(buf, "(null)") == 0);

    rc = test_vsnprintf(buf, sizeof(buf), "%lld/%llu", -12LL, 34ULL);
    TEST_ASSERT("stdio", rc == 6);
    TEST_ASSERT("stdio", strcmp(buf, "-12/34") == 0);

    rc = snprintf((char *)0, 0UL, "abc");
    TEST_ASSERT("stdio", rc == 3);

    rc = fputs("", 1);
    TEST_ASSERT("stdio", rc == 0);
    c = fputc('Z', 1);
    TEST_ASSERT("stdio", c == 'Z');
    rc = fprintf(1, "");
    TEST_ASSERT("stdio", rc == 0);
    rc = dprintf(-1, "x");
    TEST_ASSERT("stdio", rc == EOF);
}

int cleonos_app_main(void) {
    (void)puts("[libctest] start");

    test_string_lib();
    test_ctype_lib();
    test_stdlib_lib();
    test_stdio_lib();

    (void)printf("[libctest] total=%d failed=%d\n", g_total, g_failed);

    if (g_failed == 0) {
        (void)puts("[libctest] PASS");
        return 0;
    }

    (void)puts("[libctest] FAIL");
    return 1;
}
