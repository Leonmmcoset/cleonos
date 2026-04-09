# CLeonOS Stage3

## Stage Goal
- Add interrupt and exception foundation for x86_64 CLKS.
- Build and load IDT with exception vectors (0-31) and PIC IRQ vectors (32-47).
- Remap PIC and enable timer IRQ.
- Provide unified ISR stub + C dispatcher.

## Acceptance Criteria
- Kernel boots and prints `INT IDT + PIC INITIALIZED`.
- CPU exceptions are captured and logged with vector/error/RIP.
- Timer IRQ increments internal tick counter without panic.
- Kernel reaches idle loop with interrupts enabled.

## Build Targets
- `make setup`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- Triple fault right after enabling interrupts:
  - Check IDT entry selector/type and verify `lidt` loaded valid base/limit.
- Exception panic with wrong vector/error layout:
  - Verify assembly stub push order matches `struct clks_interrupt_frame`.
- IRQ storm or hang:
  - Ensure PIC EOI is sent for IRQ vectors and IRQ masks are correct.
- Link failure for ISR symbols:
  - Confirm `interrupt_stubs.S` is included in Makefile and assembled to object file.
- Limine ELF panic on segment permissions:
  - Keep linker sections page-aligned to avoid mixed permission pages.