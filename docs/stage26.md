# Stage 26 - Kernel Builtin ELF Loader + Root Hello ELF

## Goal
- Move `elfloader` from standalone user ELF into a kernel builtin command.
- Keep `hello.elf` as root-level user ELF test payload.

## Implementation
- Removed standalone user app `elfloader_main.c`.
- Added kernel shell builtin command: `elfloader [path]`.
  - Default target path: `/hello.elf`.
  - Builtin flow: `fs_read -> elf64 inspect -> exec load -> entry call -> return status`.
- Updated CMake ramdisk placement rules:
  - `hello.elf` -> `/hello.elf`
- Simplified user Rust library back to shared helper export (`cleonos_rust_guarded_len`).

## Acceptance Criteria
- No `elfloader.elf` is generated or packed.
- Ramdisk root contains `/hello.elf`.
- In kernel shell:
  - `elfloader` loads and executes `/hello.elf`, then returns status.
  - `elfloader /path/to/app.elf` works for other absolute/relative paths.

## Build Targets
- `make userapps`
- `make ramdisk`
- `make iso`
- `make run`

## QEMU Command
- `make run`

## Debug Notes
- If `elfloader` reports `file missing`, check ramdisk root packaging for `/hello.elf`.
- If it reports `invalid elf64`, verify user app link script and ELF output format.
- If it reports `exec failed`, inspect `EXEC` channel logs for load/entry/return status.
- `hello.elf` is supported in current synchronous exec mode.
- `/shell/shell.elf` is intentionally blocked in synchronous mode (interactive loop requires process/task context switching stage).
