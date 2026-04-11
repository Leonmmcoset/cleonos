# CLeonOS Stage15

## Stage Goal
- Keep ELF path unchanged and continue non-ELF stability work.
- Add PS/2 keyboard IRQ handling in kernel interrupt path.
- Support virtual terminal hotkey switching with `Alt+F1`~`Alt+F4`.
- Keep scheduler, syscall, and existing Stage14 behavior stable.

## What Was Implemented
- New keyboard module:
  - `clks/include/clks/keyboard.h`
  - `clks/kernel/keyboard.c`
- Interrupt integration:
  - Enable IRQ1 (keyboard) alongside IRQ0 in PIC mask.
  - Read scancode from PS/2 data port `0x60`.
  - Dispatch scancode to keyboard hotkey handler.
- Hotkey behavior:
  - Hold `Alt` + press `F1/F2/F3/F4` to switch TTY 0/1/2/3.
  - On successful switch, kernel logs active TTY and switch counter.
- Boot stage update:
  - `CLEONOS STAGE15 START`.

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE15 START`.
- Boot logs include keyboard module online message:
  - `[INFO][KBD] ALT+F1..F4 TTY HOTKEY ONLINE`
- In QEMU runtime:
  - Press `Alt+F2` / `Alt+F3` / `Alt+F4` / `Alt+F1`.
  - Logs show:
    - `[INFO][TTY] HOTKEY SWITCH`
    - `[INFO][TTY] ACTIVE: 0X...`
    - `[INFO][TTY] HOTKEY_SWITCHES: 0X...`

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- Hotkey has no effect:
  - Verify QEMU window is focused and keyboard input is captured.
  - Confirm logs show Stage15 keyboard online message.
- IRQ timer works but keyboard does not:
  - Re-check PIC mask includes IRQ1 (master mask should allow IRQ0 and IRQ1).
- Wrong function key mapping:
  - Current mapping is Set1 scancode: F1=0x3B, F2=0x3C, F3=0x3D, F4=0x3E.
- Excessive hotkey logs:
  - Holding key may repeat make scancode by keyboard auto-repeat; this is expected in current stage.
