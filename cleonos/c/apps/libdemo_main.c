#include <stdio.h>

typedef unsigned long long u64;

u64 cleonos_libdemo_add(u64 left, u64 right) {
    return left + right;
}

u64 cleonos_libdemo_mul(u64 left, u64 right) {
    return left * right;
}

u64 cleonos_libdemo_hello(void) {
    (void)puts("[libdemo] hello from libdemo.elf");
    return 0ULL;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)puts("[libdemo] dynamic library image ready");
    return 0;
}
