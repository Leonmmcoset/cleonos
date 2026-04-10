# CLeonOS Stage7

## Stage Goal
- Add user-space foundation under `/cleonos/` with C + Rust mixed development.
- Build first ELF app set: `/shell/shell.elf`, `/system/elfrunner.elf`, `/system/memc.elf`.
- Add user syscall wrapper (`int 0x80`) and minimal user runtime entry (`_start`).
- Integrate user ELF packaging into ramdisk build pipeline.

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE7 START`.
- FS and userland framework initialize without panic.
- Kernel logs `USERLAND FRAMEWORK ONLINE`.
- Kernel can probe and inspect all required ELF files:
  - `/shell/shell.elf`
  - `/system/elfrunner.elf`
  - `/system/memc.elf`
- `make userapps` outputs 3 ELF files and `make iso` packs them into ramdisk.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- `missing tool: rustc` in `make setup`:
  - Install Rust toolchain or set `RUSTC` to valid executable path.
- User ELF linking fails:
  - Verify `USER_CC`, `USER_LD`, and `cleonos/c/user.ld` are valid.
- `USERLAND INIT FAILED` at boot:
  - Confirm ramdisk contains `shell.elf`, `elfrunner.elf`, `memc.elf` in expected directories.
- `ELF INSPECT FAILED` on user files:
  - Ensure built apps are ELF64 and not stripped into unsupported format.
- Ramdisk missing user apps:
  - Run `make userapps` then `make iso`; check staging path `build/x86_64/ramdisk_root`.
