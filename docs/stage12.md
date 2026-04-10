# CLeonOS Stage12

## Stage Goal
- Add user execution manager in CLKS (`userland manager`) for shell launch lifecycle.
- Connect user execution state to scheduler via a dedicated kernel task (`usrd`).
- Extend syscall ABI with user execution telemetry (`ready/requested/tries/ok/fail`).
- Expose userland status as a first-class kernel service (`CLKS_SERVICE_USER`).

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE12 START`.
- Userland logs include:
  - `USERLAND FRAMEWORK ONLINE`
  - `SHELL ELF READY`
  - `SHELL EXEC REQUESTED`
- Scheduler task count increases by one (`usrd` added).
- Service framework includes USER service (service count increases).
- Stage12 syscall probe logs include:
  - `USER SHELL_READY`
  - `USER EXEC_REQUESTED`
  - `USER LAUNCH_TRIES`
  - `USER LAUNCH_OK`
  - `USER LAUNCH_FAIL`
- System remains stable in idle loop with periodic task dispatch logs.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- USER syscall counters always 0:
  - Check syscall IDs 16..20 match in kernel and user headers.
- Service count did not increase:
  - Confirm `CLKS_SERVICE_USER` is registered in `clks_service_init()`.
- `SHELL EXEC REQUEST FAILED`:
  - Ensure `/shell/shell.elf` exists in ramdisk and ELF inspect passes.
- `usrd` task missing:
  - Verify `clks_scheduler_add_kernel_task_ex("usrd", ...)` in `kmain`.
- Boot regression after Stage12 merge:
  - Re-check init order: FS -> userland init -> driver/kelf/exec -> scheduler -> service -> interrupts.
