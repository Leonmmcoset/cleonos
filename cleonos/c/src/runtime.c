#include <cleonos_syscall.h>

#define CLEONOS_RUNTIME_ARGV_MAX 24ULL
#define CLEONOS_RUNTIME_ENVP_MAX 24ULL
#define CLEONOS_RUNTIME_ITEM_MAX 128ULL

extern int cleonos_app_main(int argc, char **argv, char **envp);
extern void cleonos_cmd_runtime_pre_main(char **envp) __attribute__((weak));

u64 _start(void) {
    static char argv_items[CLEONOS_RUNTIME_ARGV_MAX][CLEONOS_RUNTIME_ITEM_MAX];
    static char env_items[CLEONOS_RUNTIME_ENVP_MAX][CLEONOS_RUNTIME_ITEM_MAX];
    static char *argv_ptrs[CLEONOS_RUNTIME_ARGV_MAX + 1ULL];
    static char *env_ptrs[CLEONOS_RUNTIME_ENVP_MAX + 1ULL];
    u64 argc = 0ULL;
    u64 envc = 0ULL;
    u64 i;
    int code;

    argc = cleonos_sys_proc_argc();
    envc = cleonos_sys_proc_envc();

    if (argc > CLEONOS_RUNTIME_ARGV_MAX) {
        argc = CLEONOS_RUNTIME_ARGV_MAX;
    }

    if (envc > CLEONOS_RUNTIME_ENVP_MAX) {
        envc = CLEONOS_RUNTIME_ENVP_MAX;
    }

    for (i = 0ULL; i < argc; i++) {
        argv_items[i][0] = '\0';
        (void)cleonos_sys_proc_argv(i, argv_items[i], CLEONOS_RUNTIME_ITEM_MAX);
        argv_ptrs[i] = argv_items[i];
    }

    argv_ptrs[argc] = (char *)0;

    for (i = 0ULL; i < envc; i++) {
        env_items[i][0] = '\0';
        (void)cleonos_sys_proc_env(i, env_items[i], CLEONOS_RUNTIME_ITEM_MAX);
        env_ptrs[i] = env_items[i];
    }

    env_ptrs[envc] = (char *)0;

    if (cleonos_cmd_runtime_pre_main != (void (*)(char **))0) {
        cleonos_cmd_runtime_pre_main(env_ptrs);
    }

    code = cleonos_app_main((int)argc, argv_ptrs, env_ptrs);
    return (u64)code;
}
