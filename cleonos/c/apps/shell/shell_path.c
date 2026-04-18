#include "shell_internal.h"

static int ush_path_push_component(char *path, u64 path_size, u64 *io_len, const char *component, u64 comp_len) {
    u64 i;

    if (path == (char *)0 || io_len == (u64 *)0 || component == (const char *)0 || comp_len == 0ULL) {
        return 0;
    }

    if (*io_len == 1ULL) {
        if (*io_len + comp_len >= path_size) {
            return 0;
        }

        for (i = 0ULL; i < comp_len; i++) {
            path[1ULL + i] = component[i];
        }

        *io_len = 1ULL + comp_len;
        path[*io_len] = '\0';
        return 1;
    }

    if (*io_len + 1ULL + comp_len >= path_size) {
        return 0;
    }

    path[*io_len] = '/';
    for (i = 0ULL; i < comp_len; i++) {
        path[*io_len + 1ULL + i] = component[i];
    }

    *io_len += (1ULL + comp_len);
    path[*io_len] = '\0';
    return 1;
}

static void ush_path_pop_component(char *path, u64 *io_len) {
    if (path == (char *)0 || io_len == (u64 *)0) {
        return;
    }

    if (*io_len <= 1ULL) {
        path[0] = '/';
        path[1] = '\0';
        *io_len = 1ULL;
        return;
    }

    while (*io_len > 1ULL && path[*io_len - 1ULL] != '/') {
        (*io_len)--;
    }

    if (*io_len > 1ULL) {
        (*io_len)--;
    }

    path[*io_len] = '\0';
}

static int ush_path_parse_into(const char *src, char *out_path, u64 out_size, u64 *io_len) {
    u64 i = 0ULL;

    if (src == (const char *)0 || out_path == (char *)0 || io_len == (u64 *)0) {
        return 0;
    }

    if (src[0] == '/') {
        i = 1ULL;
    }

    while (src[i] != '\0') {
        u64 start;
        u64 len;

        while (src[i] == '/') {
            i++;
        }

        if (src[i] == '\0') {
            break;
        }

        start = i;

        while (src[i] != '\0' && src[i] != '/') {
            i++;
        }

        len = i - start;

        if (len == 1ULL && src[start] == '.') {
            continue;
        }

        if (len == 2ULL && src[start] == '.' && src[start + 1ULL] == '.') {
            ush_path_pop_component(out_path, io_len);
            continue;
        }

        if (ush_path_push_component(out_path, out_size, io_len, src + start, len) == 0) {
            return 0;
        }
    }

    return 1;
}

int ush_resolve_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size) {
    u64 len = 1ULL;

    if (sh == (const ush_state *)0 || out_path == (char *)0 || out_size < 2ULL) {
        return 0;
    }

    out_path[0] = '/';
    out_path[1] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return ush_path_parse_into(sh->cwd, out_path, out_size, &len);
    }

    if (arg[0] != '/') {
        if (ush_path_parse_into(sh->cwd, out_path, out_size, &len) == 0) {
            return 0;
        }
    }

    return ush_path_parse_into(arg, out_path, out_size, &len);
}

int ush_resolve_exec_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size) {
    u64 i;
    u64 cursor = 0ULL;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (arg[0] == '\0') {
        return 0;
    }

    out_path[0] = '\0';

    if (arg[0] == '/') {
        ush_copy(out_path, out_size, arg);
    } else if (ush_contains_char(arg, '/') != 0) {
        if (ush_resolve_path(sh, arg, out_path, out_size) == 0) {
            return 0;
        }
    } else {
        static const char prefix[] = "/shell/";
        u64 prefix_len = (u64)(sizeof(prefix) - 1U);

        if (prefix_len + 1ULL >= out_size) {
            return 0;
        }

        for (i = 0ULL; i < prefix_len; i++) {
            out_path[cursor++] = prefix[i];
        }

        for (i = 0ULL; arg[i] != '\0'; i++) {
            if (cursor + 1ULL >= out_size) {
                return 0;
            }
            out_path[cursor++] = arg[i];
        }

        out_path[cursor] = '\0';
    }

    if (ush_has_suffix(out_path, ".elf") == 0) {
        static const char suffix[] = ".elf";

        cursor = ush_strlen(out_path);

        for (i = 0ULL; suffix[i] != '\0'; i++) {
            if (cursor + 1ULL >= out_size) {
                return 0;
            }
            out_path[cursor++] = suffix[i];
        }

        out_path[cursor] = '\0';
    }

    return 1;
}

int ush_path_is_under_system(const char *path) {
    if (path == (const char *)0) {
        return 0;
    }

    if (path[0] != '/' || path[1] != 's' || path[2] != 'y' || path[3] != 's' || path[4] != 't' || path[5] != 'e' ||
        path[6] != 'm') {
        return 0;
    }

    return (path[7] == '\0' || path[7] == '/') ? 1 : 0;
}
