# CLeonOS Stage10

## Stage Goal
- Upgrade ELF execution from "inspect-only" to "scheduler-driven kernel ELF execution".
- Build `/system/elfrunner.elf` and `/system/memc.elf` as kernel-ELF entry binaries.
- Add CLKS kernel ELF executor (`kelf`) that loads runtime image and dispatches ELF entry.
- Add `kelfd` scheduler task to periodically execute loaded kernel ELF apps.

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE10 START`.
- KELF framework logs:
  - `APP READY` for `/system/elfrunner.elf` and `/system/memc.elf`
  - `EXECUTOR ONLINE`
  - `APP_COUNT` > 0
- Service framework includes KELF service and logs updated counts.
- Scheduler task count increases (idle + klogd + kworker + kelfd).
- System remains stable in interrupt-enabled idle loop.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- `KELF APP LOAD FAILED`:
  - Check `/system/*.elf` are built using `cleonos/c/kelf.ld` and are valid ELF64.
- No `APP READY` logs:
  - Verify ramdisk staging copied system ELF files into `/system`.
- `kelfd` task missing:
  - Confirm `clks_scheduler_add_kernel_task_ex("kelfd", ...)` is present in `kmain`.
- KELF syscall counters return `-1`:
  - Ensure syscall IDs for `KELF_COUNT` and `KELF_RUNS` match kernel/user headers.
- Build errors in Makefile app targets:
  - Verify `APP_ELFRUNNER`/`APP_MEMC` link with `KELF_LDFLAGS` not `USER_LDFLAGS`.
