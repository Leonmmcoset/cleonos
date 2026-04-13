from __future__ import annotations

import argparse
import sys

from .constants import DEFAULT_MAX_EXEC_DEPTH
from .runner import CLeonOSWineNative, resolve_elf_target, resolve_rootfs
from .state import SharedKernelState


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CLeonOS-Wine: run CLeonOS ELF with Unicorn.")
    parser.add_argument("elf", help="Target ELF path. Supports /guest/path or host file path.")
    parser.add_argument("--rootfs", help="Rootfs directory (default: build/x86_64/ramdisk_root).")
    parser.add_argument("--no-kbd", action="store_true", help="Disable host keyboard input pump.")
    parser.add_argument("--max-exec-depth", type=int, default=DEFAULT_MAX_EXEC_DEPTH, help="Nested exec depth guard.")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose runner output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        rootfs = resolve_rootfs(args.rootfs)
        elf_path, guest_path = resolve_elf_target(args.elf, rootfs)
    except Exception as exc:
        print(f"[WINE][ERROR] {exc}", file=sys.stderr)
        return 2

    if args.verbose:
        print("[WINE] backend=unicorn", file=sys.stderr)
        print(f"[WINE] rootfs={rootfs}", file=sys.stderr)
        print(f"[WINE] elf={elf_path}", file=sys.stderr)
        print(f"[WINE] guest={guest_path}", file=sys.stderr)

    state = SharedKernelState()
    runner = CLeonOSWineNative(
        elf_path=elf_path,
        rootfs=rootfs,
        guest_path_hint=guest_path,
        state=state,
        max_exec_depth=max(1, args.max_exec_depth),
        no_kbd=args.no_kbd,
        verbose=args.verbose,
        top_level=True,
    )
    ret = runner.run()
    if ret is None:
        return 1

    if args.verbose:
        print(f"\n[WINE] exit=0x{ret:016X}", file=sys.stderr)
    return int(ret & 0xFF)