# CLeonOS Stage1

## Stage Goal
- Bring up Limine boot path and CLKS kernel skeleton.
- Support x86_64 and aarch64 build targets in one Makefile.
- Initialize serial log channel and framebuffer + virtual TTY output.
- Build ISO with ramdisk root layout module.

## Acceptance Criteria
- Kernel starts via Limine and reaches `clks_kernel_main`.
- Serial output shows boot logs and architecture banner.
- Framebuffer renders TTY text output for kernel logs.
- `make ARCH=x86_64 iso` and `make ARCH=aarch64 iso` produce architecture-specific ISO paths.
- Ramdisk archive is generated from `/ramdisk` and packed into boot image.

## Build Targets
- `make setup`
- `make setup LIMINE_REF=<branch-or-tag>`
- `make ARCH=x86_64 kernel`
- `make ARCH=x86_64 iso`
- `make ARCH=x86_64 run`
- `make ARCH=x86_64 debug`
- `make ARCH=aarch64 kernel`
- `make ARCH=aarch64 iso`
- `make ARCH=aarch64 run AARCH64_EFI=/path/to/QEMU_EFI.fd`

## QEMU Commands
- x86_64:
  - `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`
- aarch64:
  - `qemu-system-aarch64 -M virt -cpu cortex-a72 -m 1024M -bios QEMU_EFI.fd -cdrom build/CLeonOS-aarch64.iso -serial stdio`

## Common Bugs and Debugging
- No boot entry in Limine menu:
  - Check `configs/limine.conf` copy path and ISO folder layout under `/boot/limine`.
- Black screen but serial has output:
  - Verify framebuffer request response exists; inspect `[WARN][VIDEO]` line in serial.
- No serial output:
  - x86_64: ensure QEMU uses `-serial stdio` and COM1 init path is active.
  - aarch64: ensure `virt` machine and PL011 MMIO base `0x09000000` are used.
- Linker errors for wrong architecture:
  - Confirm `ARCH` value and matching cross toolchain (`x86_64-elf-*` or `aarch64-elf-*`).
- Setup fails while cloning Limine:
  - Check network/proxy and try `make setup LIMINE_REF=<available-ref>`.
- ISO generation failure:
  - Ensure `limine` artifacts exist and `LIMINE_DIR` points to valid files.