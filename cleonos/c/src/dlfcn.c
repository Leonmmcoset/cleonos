#include <dlfcn.h>

#include <cleonos_syscall.h>

void *dlopen(const char *path, int flags) {
    u64 handle;

    (void)flags;

    if (path == (const char *)0 || path[0] == '\0') {
        return (void *)0;
    }

    handle = cleonos_sys_dl_open(path);

    if (handle == 0ULL || handle == (u64)-1) {
        return (void *)0;
    }

    return (void *)handle;
}

void *dlsym(void *handle, const char *symbol) {
    u64 addr;

    if (handle == (void *)0 || symbol == (const char *)0 || symbol[0] == '\0') {
        return (void *)0;
    }

    addr = cleonos_sys_dl_sym((u64)handle, symbol);

    if (addr == 0ULL || addr == (u64)-1) {
        return (void *)0;
    }

    return (void *)addr;
}

int dlclose(void *handle) {
    u64 rc;

    if (handle == (void *)0) {
        return -1;
    }

    rc = cleonos_sys_dl_close((u64)handle);
    return (rc == (u64)-1) ? -1 : 0;
}
