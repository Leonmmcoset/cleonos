# Diff Details

Date : 2026-04-18 19:50:42

Directory d:\\Projects\\C\\cleonos

Total : 53 files,  4123 codes, 25 comments, 883 blanks, all 5031 lines

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [.github/workflows/build-os.yml](/.github/workflows/build-os.yml) | YAML | 28 | 1 | 1 | 30 |
| [CMakeLists.txt](/CMakeLists.txt) | CMake | 161 | 0 | 10 | 171 |
| [Makefile](/Makefile) | Makefile | 30 | 0 | 2 | 32 |
| [cleonos/c/apps/cat\_main.c](/cleonos/c/apps/cat_main.c) | C | 4 | 0 | -2 | 2 |
| [cleonos/c/apps/cmd\_runtime.c](/cleonos/c/apps/cmd_runtime.c) | C | 18 | 0 | -4 | 14 |
| [cleonos/c/apps/cmd\_runtime.h](/cleonos/c/apps/cmd_runtime.h) | C++ | 4 | 0 | 0 | 4 |
| [cleonos/c/apps/cp\_main.c](/cleonos/c/apps/cp_main.c) | C | 16 | 0 | -1 | 15 |
| [cleonos/c/apps/dltest\_main.c](/cleonos/c/apps/dltest_main.c) | C | 38 | 0 | 11 | 49 |
| [cleonos/c/apps/fsstat\_main.c](/cleonos/c/apps/fsstat_main.c) | C | 1 | 0 | 0 | 1 |
| [cleonos/c/apps/grep\_main.c](/cleonos/c/apps/grep_main.c) | C | 1 | 0 | 1 | 2 |
| [cleonos/c/apps/head\_main.c](/cleonos/c/apps/head_main.c) | C | 1 | 0 | 0 | 1 |
| [cleonos/c/apps/hello\_main.c](/cleonos/c/apps/hello_main.c) | C | -1 | 0 | 0 | -1 |
| [cleonos/c/apps/libctest\_main.c](/cleonos/c/apps/libctest_main.c) | C | 219 | 0 | 39 | 258 |
| [cleonos/c/apps/libdemo\_main.c](/cleonos/c/apps/libdemo_main.c) | C | 19 | 0 | 6 | 25 |
| [cleonos/c/apps/ls\_main.c](/cleonos/c/apps/ls_main.c) | C | 14 | 0 | 5 | 19 |
| [cleonos/c/apps/shell/shell\_cmd.c](/cleonos/c/apps/shell/shell_cmd.c) | C | 219 | 0 | 63 | 282 |
| [cleonos/c/apps/shell/shell\_external.c](/cleonos/c/apps/shell/shell_external.c) | C | 18 | 0 | 2 | 20 |
| [cleonos/c/apps/shell/shell\_input.c](/cleonos/c/apps/shell/shell_input.c) | C | 4 | 0 | 2 | 6 |
| [cleonos/c/apps/shell/shell\_internal.h](/cleonos/c/apps/shell/shell_internal.h) | C++ | 13 | 0 | 0 | 13 |
| [cleonos/c/apps/shell/shell\_util.c](/cleonos/c/apps/shell/shell_util.c) | C | -20 | 0 | -9 | -29 |
| [cleonos/c/apps/stats\_main.c](/cleonos/c/apps/stats_main.c) | C | 1 | 0 | 0 | 1 |
| [cleonos/c/apps/tail\_main.c](/cleonos/c/apps/tail_main.c) | C | 1 | 0 | 0 | 1 |
| [cleonos/c/apps/top\_main.c](/cleonos/c/apps/top_main.c) | C | -1 | 0 | -2 | -3 |
| [cleonos/c/apps/wavplay\_main.c](/cleonos/c/apps/wavplay_main.c) | C | 59 | 0 | 11 | 70 |
| [cleonos/c/apps/wc\_main.c](/cleonos/c/apps/wc_main.c) | C | 1 | 0 | 0 | 1 |
| [cleonos/c/include/cleonos\_stdio.h](/cleonos/c/include/cleonos_stdio.h) | C++ | 4 | 0 | 3 | 7 |
| [cleonos/c/include/cleonos\_syscall.h](/cleonos/c/include/cleonos_syscall.h) | C++ | 14 | 0 | 0 | 14 |
| [cleonos/c/include/ctype.h](/cleonos/c/include/ctype.h) | C++ | 14 | 0 | 3 | 17 |
| [cleonos/c/include/dlfcn.h](/cleonos/c/include/dlfcn.h) | C++ | 6 | 0 | 3 | 9 |
| [cleonos/c/include/stdio.h](/cleonos/c/include/stdio.h) | C++ | 21 | 0 | 7 | 28 |
| [cleonos/c/include/stdlib.h](/cleonos/c/include/stdlib.h) | C++ | 27 | 0 | 10 | 37 |
| [cleonos/c/include/string.h](/cleonos/c/include/string.h) | C++ | 25 | 0 | 5 | 30 |
| [cleonos/c/src/dlfcn.c](/cleonos/c/src/dlfcn.c) | C | 33 | 0 | 16 | 49 |
| [cleonos/c/src/libc\_ctype.c](/cleonos/c/src/libc_ctype.c) | C | 34 | 0 | 12 | 46 |
| [cleonos/c/src/libc\_stdlib.c](/cleonos/c/src/libc_stdlib.c) | C | 198 | 0 | 57 | 255 |
| [cleonos/c/src/libc\_string.c](/cleonos/c/src/libc_string.c) | C | 340 | 0 | 106 | 446 |
| [cleonos/c/src/runtime.c](/cleonos/c/src/runtime.c) | C | 4 | 0 | 1 | 5 |
| [cleonos/c/src/stdio.c](/cleonos/c/src/stdio.c) | C | 393 | 0 | 108 | 501 |
| [cleonos/c/src/syscall.c](/cleonos/c/src/syscall.c) | C | 28 | 0 | 7 | 35 |
| [clks/include/clks/exec.h](/clks/include/clks/exec.h) | C++ | 10 | 0 | 0 | 10 |
| [clks/include/clks/syscall.h](/clks/include/clks/syscall.h) | C++ | 4 | 0 | 0 | 4 |
| [clks/include/clks/tty.h](/clks/include/clks/tty.h) | C++ | 1 | 0 | 0 | 1 |
| [clks/kernel/exec.c](/clks/kernel/exec.c) | C | 515 | 0 | 120 | 635 |
| [clks/kernel/fs.c](/clks/kernel/fs.c) | C | 3 | 0 | 2 | 5 |
| [clks/kernel/keyboard.c](/clks/kernel/keyboard.c) | C | 28 | 0 | 8 | 36 |
| [clks/kernel/kmain.c](/clks/kernel/kmain.c) | C | 188 | 0 | 25 | 213 |
| [clks/kernel/shell.c](/clks/kernel/shell.c) | C | 1 | 0 | 0 | 1 |
| [clks/kernel/syscall.c](/clks/kernel/syscall.c) | C | 108 | 0 | 31 | 139 |
| [clks/kernel/tty.c](/clks/kernel/tty.c) | C | 73 | 0 | 15 | 88 |
| [clks/kernel/userland.c](/clks/kernel/userland.c) | C | 21 | 0 | 3 | 24 |
| [configs/menuconfig/clks\_features.json](/configs/menuconfig/clks_features.json) | JSON | 196 | 0 | 1 | 197 |
| [ramdisk/dev/random](/ramdisk/dev/random) | C++ | 0 | 0 | 1 | 1 |
| [scripts/menuconfig.py](/scripts/menuconfig.py) | Python | 986 | 24 | 204 | 1,214 |

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details