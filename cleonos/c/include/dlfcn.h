#ifndef CLEONOS_LIBC_DLFCN_H
#define CLEONOS_LIBC_DLFCN_H

void *dlopen(const char *path, int flags);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);

#endif
