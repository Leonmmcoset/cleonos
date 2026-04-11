# CLeonOS Stage24

## Stage Goal
- Add a kernel shell command `panic`.
- Trigger a deliberate kernel panic from shell command input.
- Show a dedicated panic screen in framebuffer mode for manual panic and CPU exceptions.

## What Was Implemented
- New panic subsystem:
  - `clks/include/clks/panic.h`
  - `clks/kernel/panic.c`
- Panic subsystem behavior:
  - disable interrupts before panic halt
  - write panic summary to serial (`-serial stdio`)
  - draw full-screen panic page on framebuffer
  - halt forever after panic
- Exception path integration:
  - `clks/kernel/interrupts.c` now routes `vector < 32` to `clks_panic_exception(...)`
- Shell integration:
  - `clks/kernel/shell.c` adds `panic` command
  - `panic` command calls `clks_panic("MANUAL PANIC FROM KERNEL SHELL")`
  - `help` output includes `panic`
- Stage banner updated:
  - `CLEONOS Stage24 START`

## Acceptance Criteria
- Kernel boots and prints `CLEONOS Stage24 START`.
- `help` lists `panic`.
- Typing `panic` in kernel shell shows panic page and system halts.
- Triggering CPU exceptions (if any) also enters the same panic screen flow.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- `panic` command not found:
  - Confirm `help` output contains `panic` and shell dispatch includes `clks_shell_cmd_panic()`.
- Panic screen not visible but serial has panic logs:
  - Ensure framebuffer is available from Limine and `clks_fb_ready()` is true.
- Exception still prints old logs instead of panic page:
  - Check `clks_interrupt_dispatch()` exception branch routes to `clks_panic_exception(...)`.
