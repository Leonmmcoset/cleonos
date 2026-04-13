#include "shell/shell_internal.h"

int cleonos_app_main(void) {
    ush_state sh;
    char line[USH_LINE_MAX];

    ush_init_state(&sh);
    ush_writeln("[USER][SHELL] interactive framework online");

    if (ush_run_script_file(&sh, "/shell/init.cmd") == 0) {
        ush_writeln("[USER][SHELL] /shell/init.cmd missing");
    }

    for (;;) {
        ush_read_line(&sh, line, (u64)sizeof(line));
        ush_execute_line(&sh, line);

        if (sh.exit_requested != 0) {
            return (int)(sh.exit_code & 0x7FFFFFFFULL);
        }
    }
}