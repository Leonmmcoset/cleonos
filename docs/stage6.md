# CLeonOS Stage6

## Stage Goal
- Add ramdisk filesystem foundation in CLKS based on the Limine module.
- Parse tar-format ramdisk and build hierarchical directory/file nodes.
- Provide VFS-style path interfaces for stat/read/list operations.
- Enforce required root layout: `/system`, `/shell`, `/temp`, `/driver`.

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE6 START`.
- Filesystem logs `RAMDISK VFS ONLINE` and node/file statistics.
- Root layout validation reports `/SYSTEM /SHELL /TEMP /DRIVER OK`.
- `clks_fs_count_children("/")` returns non-zero and is logged.
- Kernel continues to scheduler/ELF/syscall/interrupt init without panic.

## Build Targets
- `make setup`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- `NO RAMDISK MODULE FROM LIMINE`:
  - Verify `module_path: boot():/boot/cleonos_ramdisk.tar` exists in `configs/limine.conf`.
- `RAMDISK TAR PARSE FAILED`:
  - Ensure ramdisk is packed as tar (`make ramdisk`) and module size is not zero.
- `MISSING REQUIRED DIRECTORY`:
  - Confirm ramdisk root contains `/system`, `/shell`, `/temp`, `/driver`.
- Filesystem APIs always fail:
  - Check `clks_fs_init()` is called and `clks_fs_is_ready()` is true.
- Build failure on new symbols:
  - Confirm `ramdisk.c` and `fs.c` are present in `C_SOURCES`.
