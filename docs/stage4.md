# CLeonOS Stage4

## Stage Goal
- Add process/task scheduling foundation in CLKS.
- Introduce task control blocks and kernel task registry.
- Implement timer-driven round-robin time-slice scheduling logic.
- Connect scheduler tick updates to IRQ0 interrupt path.

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE4 START`.
- Scheduler initializes and logs task count.
- At least 3 kernel tasks exist (`idle`, `klogd`, `kworker`).
- Timer IRQ invokes scheduler tick handler without panic/reboot.
- System stays in idle loop with interrupts and scheduler active.

## Build Targets
- `make setup`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- Immediate reboot after enabling interrupts:
  - Verify IDT selector uses runtime `CS` and ISR stubs are linked.
- Scheduler task count is 0:
  - Confirm `clks_scheduler_init()` is called before idle loop.
- Build failure on ISR symbols:
  - Ensure `interrupt_stubs.S` is included and `.S` build rule exists.
- Build failure on scheduler symbols:
  - Ensure `scheduler.c` is in `C_SOURCES` and `scheduler.h` is included where needed.
- No periodic scheduling activity:
  - Verify IRQ0 is unmasked and timer vector 32 path calls scheduler tick handler.