#include <dlfcn.h>
#include <stdio.h>

typedef unsigned long long u64;
typedef u64 (*dl_math2_fn)(u64, u64);
typedef u64 (*dl_void_fn)(void);

int cleonos_app_main(int argc, char **argv, char **envp) {
    const char *lib_path = "/shell/libdemo.elf";
    void *handle;
    dl_math2_fn add_fn;
    dl_math2_fn mul_fn;
    dl_void_fn hello_fn;

    (void)envp;

    if (argc > 1 && argv != (char **)0 && argv[1] != (char *)0 && argv[1][0] != '\0') {
        lib_path = argv[1];
    }

    handle = dlopen(lib_path, 0);
    if (handle == (void *)0) {
        (void)printf("[dltest] dlopen failed: %s\n", lib_path);
        return 1;
    }

    add_fn = (dl_math2_fn)dlsym(handle, "cleonos_libdemo_add");
    mul_fn = (dl_math2_fn)dlsym(handle, "cleonos_libdemo_mul");
    hello_fn = (dl_void_fn)dlsym(handle, "cleonos_libdemo_hello");

    if (add_fn == (dl_math2_fn)0 || mul_fn == (dl_math2_fn)0 || hello_fn == (dl_void_fn)0) {
        (void)puts("[dltest] dlsym failed");
        (void)dlclose(handle);
        return 2;
    }

    (void)hello_fn();
    (void)printf("[dltest] add(7, 35) = %llu\n", add_fn(7ULL, 35ULL));
    (void)printf("[dltest] mul(6, 9) = %llu\n", mul_fn(6ULL, 9ULL));

    if (dlclose(handle) != 0) {
        (void)puts("[dltest] dlclose failed");
        return 3;
    }

    (void)puts("[dltest] PASS");
    return 0;
}
