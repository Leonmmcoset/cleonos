# Stage 29 - Async Exec Dispatch + TTY-Bound Input Ownership

## Goal
- Make process launch lifecycle explicit (`pending -> running -> exited`) and tick-driven.
- Bind user input to the process TTY context to avoid cross-TTY input stealing.
- Keep kernel shell input compatibility while preparing cleaner multi-TTY user-shell behavior.

## Implementation
- Exec lifecycle (Stage 29A):
  - Added process states: `CLKS_EXEC_PROC_PENDING`, `CLKS_EXEC_PROC_RUNNING`, `CLKS_EXEC_PROC_EXITED`.
  - `clks_exec_spawn_path()` now queues process records as pending.
  - Added `clks_exec_tick()` to dispatch pending processes from scheduler tick context.
  - `waitpid` now understands pending/running/exited transitions.
- TTY ownership (Stage 29B):
  - Added `tty_index` to exec process record and capture active TTY when process is created.
  - Added `clks_exec_current_tty()` to resolve the current execution TTY safely.
  - Refactored keyboard input buffer to per-TTY ring queues.
  - Added `clks_keyboard_pop_char_for_tty()` API.
  - Updated syscall keyboard read path to pop from `clks_exec_current_tty()` queue.

## Acceptance Criteria
- `spawn` no longer blocks immediate caller flow and enters pending lifecycle.
- Tick loop dispatches pending processes without recursive launch storm.
- Keyboard input on one TTY is not consumed by process context on another TTY.
- Kernel shell input remains functional through existing `clks_keyboard_pop_char()` API.

## Build Targets
- `make userapps`
- `make ramdisk`
- `make iso`
- `make run`

## QEMU Command
- `make run`

## Debug Notes
- If user shell appears to miss input, log `clks_exec_current_tty()` and active TTY during syscall `KBD_GET_CHAR`.
- If input is delayed, inspect queue depth via `kbdstat` and verify per-TTY counters move.
- If `waitpid` stalls forever, check whether `clks_exec_tick()` is called in scheduler/user service tick.
