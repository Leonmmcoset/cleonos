# CLeonOS Stage5

## Stage Goal
- Add ELF64 parsing and loading framework for CLKS.
- Add executable-file probe path via Limine executable request.
- Add syscall framework with `int 0x80` dispatcher.
- Keep scheduler + interrupt flow integrated with syscall path.

## Acceptance Criteria
- Kernel logs `CLEONOS STAGE5 START`.
- ELF runner initializes and probes kernel ELF metadata (entry/phnum/segments).
- Syscall framework initializes and `int 0x80` returns timer ticks.
- IRQ timer and scheduler still run without panic/reboot.

## Build Targets
- `make setup`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- `KERNEL ELF PROBE FAILED`:
  - Verify Limine executable file request is present in `.limine_requests` section.
- Syscall always returns `-1`:
  - Check `clks_syscall_init()` call order and vector `0x80` IDT gate.
- `int 0x80` causes exception:
  - Ensure vector 128 stub exists and IDT gate uses DPL3-capable flags (`0xEE`).
- Linker undefined symbols for ELF/syscall:
  - Confirm `elf64.c`, `elfrunner.c`, `syscall.c` are listed in `C_SOURCES`.
- Interrupt instability after syscall integration:
  - Confirm timer IRQ EOI path is untouched and syscall vector path returns early.