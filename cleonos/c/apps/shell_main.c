#include "shell/shell_internal.h"

int cleonos_app_main(void) {
    ush_state sh;
    char line[USH_LINE_MAX];

    ush_init_state(&sh);
    ush_writeln("\x1B[92m[USER][SHELL]\x1B[0m interactive framework online");

    if (ush_run_script_file(&sh, "/shell/init.cmd") == 0 && ush_run_script_file(&sh, "/shell/INIT.CMD") == 0 &&
        ush_run_script_file(&sh, "/SHELL/INIT.CMD") == 0) {
        ush_writeln("\x1B[33m[USER][SHELL]\x1B[0m init script not found, continue interactive mode");
    }

    for (;;) {
        ush_read_line(&sh, line, (u64)sizeof(line));
        ush_execute_line(&sh, line);

        if (sh.exit_requested != 0) {
            return (int)(sh.exit_code & 0x7FFFFFFFULL);
        }
    }
}
