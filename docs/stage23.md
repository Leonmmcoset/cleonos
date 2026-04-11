# CLeonOS Stage23

## Stage Goal
- Add a kernel shell command `rusttest`.
- Implement `rusttest` logic in Rust (`no_std`) instead of C.
- Link Rust object into the kernel ELF so command runs in kernel mode.

## What Was Implemented
- Kernel Rust source added:
  - `clks/rust/src/lib.rs`
  - exports `clks_rusttest_hello()`
  - output text: `Hello world!`
- Kernel build integration (CMake):
  - build `libclks_kernel_rust.a`
  - link kernel ELF with `${KERNEL_RUST_LIB}`
- Shell integration:
  - add command `rusttest`
  - `rusttest` calls Rust symbol `clks_rusttest_hello()`
- Compatibility symbols for Rust link:
  - add `clks/lib/libc_compat.c` for `memcpy/memset/memmove/memcmp/bcmp`
- Stage banner updated:
  - `CLEONOS Stage23 START`

## Acceptance Criteria
- Kernel boots and prints `CLEONOS Stage23 START`.
- `help` contains `rusttest`.
- Running `rusttest` prints exactly:
  - `Hello world!`
- No regression to existing shell commands.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- Rust link undefined symbols (`memcpy/memset/...`):
  - Ensure `clks/lib/libc_compat.c` is included in kernel source set.
- Panic-related undefined symbol:
  - Ensure Rust file keeps `#[panic_handler]` and `rust_eh_personality` stub.
- `rusttest` command not found:
  - Confirm `help` output includes `rusttest` and shell dispatch branch exists.