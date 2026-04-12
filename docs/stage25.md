# CLeonOS Stage25

## Stage Goal
- Make `tty2` (Alt+F2) enter a basic desktop environment.
- Add first-step mouse support (PS/2 mouse IRQ and on-screen pointer).
- Keep kernel shell on `tty1` (Alt+F1) unchanged.

## What Was Implemented
- New kernel modules:
  - `clks/kernel/mouse.c`
  - `clks/kernel/desktop.c`
- New public headers:
  - `clks/include/clks/mouse.h`
  - `clks/include/clks/desktop.h`
- Interrupt and PIC updates:
  - Enable PIC cascade + mouse IRQ line.
  - Route IRQ12 (`vector 44`) to mouse byte handler.
- PS/2 mouse bring-up:
  - Enable aux device.
  - Set controller config for mouse IRQ.
  - Send mouse commands (`F6`, `F4`) and verify ACK.
  - Decode 3-byte PS/2 packets into X/Y/buttons.
- Desktop on tty2:
  - `clks_desktop_tick()` renders a simple graphical desktop scene.
  - Draw software mouse cursor and button state indicator.
  - Refresh when pointer changes and periodically to keep desktop clean.
- TTY adjustments:
  - Disable text cursor blinking on `tty2` to avoid desktop overlay artifacts.
- Keyboard routing adjustment:
  - Shell input queue only accepts character input while active TTY is `tty1` (index 0).
  - Alt+F1..F4 switching remains available globally.
- Framebuffer primitives:
  - Added `clks_fb_draw_pixel()` and `clks_fb_fill_rect()` for desktop rendering.
- Kernel flow integration:
  - Stage banner -> `CLEONOS Stage25 START`.
  - Initialize mouse + desktop during boot.
  - Call desktop tick in `usrd` task.

## Acceptance Criteria
- Boot log shows `CLEONOS Stage25 START`.
- Alt+F1 enters shell TTY; shell input works as before.
- Alt+F2 switches to desktop view (non-text UI).
- Mouse movement updates on-screen pointer in tty2.
- Left mouse button changes pointer/indicator state.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- Desktop not visible after Alt+F2:
  - Check `clks_desktop_init()` log lines and confirm framebuffer is ready.
- Mouse pointer does not move:
  - Check mouse init logs (`PS2 POINTER ONLINE`) and IRQ12 routing in `interrupts.c`.
- Frequent text artifacts on desktop:
  - Ensure desktop periodic redraw is active in `clks_desktop_tick()`.
- Shell unexpectedly receives input while on desktop:
  - Verify keyboard routing guard uses `clks_tty_active() == 0`.
