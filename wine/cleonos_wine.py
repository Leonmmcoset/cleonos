#!/usr/bin/env python3
"""
CLeonOS-Wine (native Unicorn backend)

A lightweight user-mode runner for CLeonOS x86_64 ELF applications.
This version does NOT depend on qiling.
"""

from __future__ import annotations

import argparse
import collections
import os
import struct
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Deque, List, Optional, Tuple

try:
    from unicorn import Uc, UcError
    from unicorn import UC_ARCH_X86, UC_MODE_64
    from unicorn import UC_HOOK_CODE, UC_HOOK_INTR
    from unicorn import UC_PROT_ALL, UC_PROT_EXEC, UC_PROT_READ, UC_PROT_WRITE
    from unicorn.x86_const import (
        UC_X86_REG_RAX,
        UC_X86_REG_RBX,
        UC_X86_REG_RCX,
        UC_X86_REG_RDX,
        UC_X86_REG_RBP,
        UC_X86_REG_RSP,
    )
except Exception as exc:
    print("[WINE][ERROR] unicorn import failed. Install dependencies first:", file=sys.stderr)
    print("  pip install -r wine/requirements.txt", file=sys.stderr)
    raise SystemExit(1) from exc


U64_MASK = (1 << 64) - 1
PAGE_SIZE = 0x1000
MAX_CSTR = 4096
MAX_IO_READ = 1 << 20
DEFAULT_MAX_EXEC_DEPTH = 6
FS_NAME_MAX = 96

# CLeonOS syscall IDs from cleonos/c/include/cleonos_syscall.h
SYS_LOG_WRITE = 0
SYS_TIMER_TICKS = 1
SYS_TASK_COUNT = 2
SYS_CUR_TASK = 3
SYS_SERVICE_COUNT = 4
SYS_SERVICE_READY_COUNT = 5
SYS_CONTEXT_SWITCHES = 6
SYS_KELF_COUNT = 7
SYS_KELF_RUNS = 8
SYS_FS_NODE_COUNT = 9
SYS_FS_CHILD_COUNT = 10
SYS_FS_GET_CHILD_NAME = 11
SYS_FS_READ = 12
SYS_EXEC_PATH = 13
SYS_EXEC_REQUESTS = 14
SYS_EXEC_SUCCESS = 15
SYS_USER_SHELL_READY = 16
SYS_USER_EXEC_REQUESTED = 17
SYS_USER_LAUNCH_TRIES = 18
SYS_USER_LAUNCH_OK = 19
SYS_USER_LAUNCH_FAIL = 20
SYS_TTY_COUNT = 21
SYS_TTY_ACTIVE = 22
SYS_TTY_SWITCH = 23
SYS_TTY_WRITE = 24
SYS_TTY_WRITE_CHAR = 25
SYS_KBD_GET_CHAR = 26


def u64(value: int) -> int:
    return value & U64_MASK


def u64_neg1() -> int:
    return U64_MASK


def page_floor(addr: int) -> int:
    return addr & ~(PAGE_SIZE - 1)


def page_ceil(addr: int) -> int:
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)


@dataclass
class ELFSegment:
    vaddr: int
    memsz: int
    flags: int
    data: bytes


@dataclass
class ELFImage:
    entry: int
    segments: List[ELFSegment]


@dataclass
class SharedKernelState:
    start_ns: int = field(default_factory=time.monotonic_ns)
    task_count: int = 5
    current_task: int = 0
    service_count: int = 7
    service_ready: int = 7
    context_switches: int = 0
    kelf_count: int = 2
    kelf_runs: int = 0
    exec_requests: int = 0
    exec_success: int = 0
    user_shell_ready: int = 1
    user_exec_requested: int = 0
    user_launch_tries: int = 0
    user_launch_ok: int = 0
    user_launch_fail: int = 0
    tty_count: int = 4
    tty_active: int = 0
    kbd_queue: Deque[int] = field(default_factory=collections.deque)
    kbd_lock: threading.Lock = field(default_factory=threading.Lock)

    def timer_ticks(self) -> int:
        return (time.monotonic_ns() - self.start_ns) // 1_000_000

    def push_key(self, key: int) -> None:
        with self.kbd_lock:
            if len(self.kbd_queue) >= 1024:
                self.kbd_queue.popleft()
            self.kbd_queue.append(key & 0xFF)

    def pop_key(self) -> Optional[int]:
        with self.kbd_lock:
            if not self.kbd_queue:
                return None
            return self.kbd_queue.popleft()


class InputPump:
    def __init__(self, state: SharedKernelState) -> None:
        self.state = state
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._posix_term_state = None

    def start(self) -> None:
        if self._thread is not None:
            return
        if not sys.stdin or not hasattr(sys.stdin, "isatty") or not sys.stdin.isatty():
            return
        self._thread = threading.Thread(target=self._run, name="cleonos-wine-input", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=0.2)
            self._thread = None
        self._restore_posix_tty()

    def _run(self) -> None:
        if os.name == "nt":
            self._run_windows()
        else:
            self._run_posix()

    def _run_windows(self) -> None:
        import msvcrt  # pylint: disable=import-error

        while not self._stop.is_set():
            if not msvcrt.kbhit():
                time.sleep(0.005)
                continue

            ch = msvcrt.getwch()
            if ch in ("\x00", "\xe0"):
                _ = msvcrt.getwch()
                continue

            norm = self._normalize_char(ch)
            if norm is None:
                continue
            self.state.push_key(ord(norm))

    def _run_posix(self) -> None:
        import select
        import termios
        import tty

        fd = sys.stdin.fileno()
        self._posix_term_state = termios.tcgetattr(fd)
        tty.setcbreak(fd)

        try:
            while not self._stop.is_set():
                readable, _, _ = select.select([sys.stdin], [], [], 0.05)
                if not readable:
                    continue
                ch = sys.stdin.read(1)
                norm = self._normalize_char(ch)
                if norm is None:
                    continue
                self.state.push_key(ord(norm))
        finally:
            self._restore_posix_tty()

    def _restore_posix_tty(self) -> None:
        if self._posix_term_state is None:
            return
        try:
            import termios

            fd = sys.stdin.fileno()
            termios.tcsetattr(fd, termios.TCSADRAIN, self._posix_term_state)
        except Exception:
            pass
        finally:
            self._posix_term_state = None

    @staticmethod
    def _normalize_char(ch: str) -> Optional[str]:
        if not ch:
            return None
        if ch == "\r":
            return "\n"
        return ch


class CLeonOSWineNative:
    def __init__(
        self,
        elf_path: Path,
        rootfs: Path,
        guest_path_hint: str,
        *,
        state: Optional[SharedKernelState] = None,
        depth: int = 0,
        max_exec_depth: int = DEFAULT_MAX_EXEC_DEPTH,
        no_kbd: bool = False,
        verbose: bool = False,
        top_level: bool = True,
    ) -> None:
        self.elf_path = elf_path
        self.rootfs = rootfs
        self.guest_path_hint = guest_path_hint
        self.state = state if state is not None else SharedKernelState()
        self.depth = depth
        self.max_exec_depth = max_exec_depth
        self.no_kbd = no_kbd
        self.verbose = verbose
        self.top_level = top_level

        self.image = self._parse_elf(self.elf_path)
        self.exit_code: Optional[int] = None
        self._input_pump: Optional[InputPump] = None

        self._stack_base = 0x00007FFF00000000
        self._stack_size = 0x0000000000020000
        self._ret_sentinel = 0x00007FFF10000000
        self._mapped_ranges: List[Tuple[int, int]] = []

    def run(self) -> Optional[int]:
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self._install_hooks(uc)
        self._load_segments(uc)
        self._prepare_stack_and_return(uc)

        if self.top_level and not self.no_kbd:
            self._input_pump = InputPump(self.state)
            self._input_pump.start()

        try:
            uc.emu_start(self.image.entry, 0)
        except KeyboardInterrupt:
            if self.top_level:
                print("\n[WINE] interrupted by user", file=sys.stderr)
            return None
        except UcError as exc:
            if self.verbose or self.top_level:
                print(f"[WINE][ERROR] runtime crashed: {exc}", file=sys.stderr)
            return None
        finally:
            if self.top_level and self._input_pump is not None:
                self._input_pump.stop()

        if self.exit_code is None:
            self.exit_code = self._reg_read(uc, UC_X86_REG_RAX)

        return u64(self.exit_code)

    def _install_hooks(self, uc: Uc) -> None:
        uc.hook_add(UC_HOOK_INTR, self._hook_intr)
        uc.hook_add(UC_HOOK_CODE, self._hook_code, begin=self._ret_sentinel, end=self._ret_sentinel)

    def _hook_code(self, uc: Uc, address: int, size: int, _user_data) -> None:
        _ = size
        if address == self._ret_sentinel:
            self.exit_code = self._reg_read(uc, UC_X86_REG_RAX)
            uc.emu_stop()

    def _hook_intr(self, uc: Uc, intno: int, _user_data) -> None:
        if intno != 0x80:
            raise UcError(1)

        syscall_id = self._reg_read(uc, UC_X86_REG_RAX)
        arg0 = self._reg_read(uc, UC_X86_REG_RBX)
        arg1 = self._reg_read(uc, UC_X86_REG_RCX)
        arg2 = self._reg_read(uc, UC_X86_REG_RDX)

        self.state.context_switches = u64(self.state.context_switches + 1)
        ret = self._dispatch_syscall(uc, syscall_id, arg0, arg1, arg2)
        self._reg_write(uc, UC_X86_REG_RAX, u64(ret))

    def _dispatch_syscall(self, uc: Uc, sid: int, arg0: int, arg1: int, arg2: int) -> int:
        if sid == SYS_LOG_WRITE:
            data = self._read_guest_bytes(uc, arg0, arg1)
            self._host_write(data.decode("utf-8", errors="replace"))
            return len(data)
        if sid == SYS_TIMER_TICKS:
            return self.state.timer_ticks()
        if sid == SYS_TASK_COUNT:
            return self.state.task_count
        if sid == SYS_CUR_TASK:
            return self.state.current_task
        if sid == SYS_SERVICE_COUNT:
            return self.state.service_count
        if sid == SYS_SERVICE_READY_COUNT:
            return self.state.service_ready
        if sid == SYS_CONTEXT_SWITCHES:
            return self.state.context_switches
        if sid == SYS_KELF_COUNT:
            return self.state.kelf_count
        if sid == SYS_KELF_RUNS:
            return self.state.kelf_runs
        if sid == SYS_FS_NODE_COUNT:
            return self._fs_node_count()
        if sid == SYS_FS_CHILD_COUNT:
            return self._fs_child_count(uc, arg0)
        if sid == SYS_FS_GET_CHILD_NAME:
            return self._fs_get_child_name(uc, arg0, arg1, arg2)
        if sid == SYS_FS_READ:
            return self._fs_read(uc, arg0, arg1, arg2)
        if sid == SYS_EXEC_PATH:
            return self._exec_path(uc, arg0)
        if sid == SYS_EXEC_REQUESTS:
            return self.state.exec_requests
        if sid == SYS_EXEC_SUCCESS:
            return self.state.exec_success
        if sid == SYS_USER_SHELL_READY:
            return self.state.user_shell_ready
        if sid == SYS_USER_EXEC_REQUESTED:
            return self.state.user_exec_requested
        if sid == SYS_USER_LAUNCH_TRIES:
            return self.state.user_launch_tries
        if sid == SYS_USER_LAUNCH_OK:
            return self.state.user_launch_ok
        if sid == SYS_USER_LAUNCH_FAIL:
            return self.state.user_launch_fail
        if sid == SYS_TTY_COUNT:
            return self.state.tty_count
        if sid == SYS_TTY_ACTIVE:
            return self.state.tty_active
        if sid == SYS_TTY_SWITCH:
            if arg0 >= self.state.tty_count:
                return u64_neg1()
            self.state.tty_active = int(arg0)
            return 0
        if sid == SYS_TTY_WRITE:
            data = self._read_guest_bytes(uc, arg0, arg1)
            self._host_write(data.decode("utf-8", errors="replace"))
            return len(data)
        if sid == SYS_TTY_WRITE_CHAR:
            ch = chr(arg0 & 0xFF)
            if ch in ("\b", "\x7f"):
                self._host_write("\b \b")
            else:
                self._host_write(ch)
            return 0
        if sid == SYS_KBD_GET_CHAR:
            key = self.state.pop_key()
            return u64_neg1() if key is None else key

        return u64_neg1()

    def _host_write(self, text: str) -> None:
        if not text:
            return
        sys.stdout.write(text)
        sys.stdout.flush()

    def _load_segments(self, uc: Uc) -> None:
        for seg in self.image.segments:
            start = page_floor(seg.vaddr)
            end = page_ceil(seg.vaddr + seg.memsz)
            self._map_region(uc, start, end - start, UC_PROT_ALL)

        for seg in self.image.segments:
            if seg.data:
                self._mem_write(uc, seg.vaddr, seg.data)

        # Try to tighten protections after data is in place.
        for seg in self.image.segments:
            start = page_floor(seg.vaddr)
            end = page_ceil(seg.vaddr + seg.memsz)
            size = end - start
            perms = 0
            if seg.flags & 0x4:
                perms |= UC_PROT_READ
            if seg.flags & 0x2:
                perms |= UC_PROT_WRITE
            if seg.flags & 0x1:
                perms |= UC_PROT_EXEC
            if perms == 0:
                perms = UC_PROT_READ
            try:
                uc.mem_protect(start, size, perms)
            except Exception:
                pass

    def _prepare_stack_and_return(self, uc: Uc) -> None:
        self._map_region(uc, self._stack_base, self._stack_size, UC_PROT_READ | UC_PROT_WRITE)
        self._map_region(uc, self._ret_sentinel, PAGE_SIZE, UC_PROT_READ | UC_PROT_EXEC)
        self._mem_write(uc, self._ret_sentinel, b"\x90")

        rsp = self._stack_base + self._stack_size - 8
        self._mem_write(uc, rsp, struct.pack("<Q", self._ret_sentinel))

        self._reg_write(uc, UC_X86_REG_RSP, rsp)
        self._reg_write(uc, UC_X86_REG_RBP, rsp)

    def _map_region(self, uc: Uc, addr: int, size: int, perms: int) -> None:
        if size <= 0:
            return
        start = page_floor(addr)
        end = page_ceil(addr + size)

        if self._is_range_mapped(start, end):
            return

        uc.mem_map(start, end - start, perms)
        self._mapped_ranges.append((start, end))

    def _is_range_mapped(self, start: int, end: int) -> bool:
        for ms, me in self._mapped_ranges:
            if start >= ms and end <= me:
                return True
        return False

    @staticmethod
    def _reg_read(uc: Uc, reg: int) -> int:
        return int(uc.reg_read(reg))

    @staticmethod
    def _reg_write(uc: Uc, reg: int, value: int) -> None:
        uc.reg_write(reg, u64(value))

    @staticmethod
    def _mem_write(uc: Uc, addr: int, data: bytes) -> None:
        if addr == 0 or not data:
            return
        uc.mem_write(addr, data)

    def _read_guest_cstring(self, uc: Uc, addr: int, max_len: int = MAX_CSTR) -> str:
        if addr == 0:
            return ""

        out = bytearray()
        for i in range(max_len):
            try:
                ch = uc.mem_read(addr + i, 1)
            except UcError:
                break
            if not ch or ch[0] == 0:
                break
            out.append(ch[0])
        return out.decode("utf-8", errors="replace")

    def _read_guest_bytes(self, uc: Uc, addr: int, size: int) -> bytes:
        if addr == 0 or size == 0:
            return b""
        safe_size = int(min(size, MAX_IO_READ))
        try:
            return bytes(uc.mem_read(addr, safe_size))
        except UcError:
            return b""

    def _write_guest_bytes(self, uc: Uc, addr: int, data: bytes) -> bool:
        if addr == 0:
            return False
        try:
            uc.mem_write(addr, data)
            return True
        except UcError:
            return False

    @staticmethod
    def _parse_elf(path: Path) -> ELFImage:
        data = path.read_bytes()
        if len(data) < 64:
            raise RuntimeError(f"ELF too small: {path}")
        if data[0:4] != b"\x7fELF":
            raise RuntimeError(f"invalid ELF magic: {path}")
        if data[4] != 2 or data[5] != 1:
            raise RuntimeError(f"unsupported ELF class/endianness: {path}")

        entry = struct.unpack_from("<Q", data, 0x18)[0]
        phoff = struct.unpack_from("<Q", data, 0x20)[0]
        phentsize = struct.unpack_from("<H", data, 0x36)[0]
        phnum = struct.unpack_from("<H", data, 0x38)[0]

        if entry == 0:
            raise RuntimeError(f"ELF entry is 0: {path}")
        if phentsize == 0 or phnum == 0:
            raise RuntimeError(f"ELF has no program headers: {path}")

        segments: List[ELFSegment] = []
        for i in range(phnum):
            off = phoff + i * phentsize
            if off + 56 > len(data):
                break

            p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = struct.unpack_from(
                "<IIQQQQQQ", data, off
            )

            if p_type != 1 or p_memsz == 0:
                continue

            fs = int(p_filesz)
            fo = int(p_offset)
            if fs > 0:
                if fo >= len(data):
                    seg_data = b""
                else:
                    seg_data = data[fo : min(len(data), fo + fs)]
            else:
                seg_data = b""

            segments.append(ELFSegment(vaddr=int(p_vaddr), memsz=int(p_memsz), flags=int(p_flags), data=seg_data))

        if not segments:
            raise RuntimeError(f"ELF has no PT_LOAD segments: {path}")

        return ELFImage(entry=int(entry), segments=segments)

    def _fs_node_count(self) -> int:
        count = 1
        for _root, dirs, files in os.walk(self.rootfs):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            files = [f for f in files if not f.startswith(".")]
            count += len(dirs) + len(files)
        return count

    def _fs_child_count(self, uc: Uc, dir_ptr: int) -> int:
        path = self._read_guest_cstring(uc, dir_ptr)
        host_dir = self._guest_to_host(path, must_exist=True)
        if host_dir is None or not host_dir.is_dir():
            return u64_neg1()
        return len(self._list_children(host_dir))

    def _fs_get_child_name(self, uc: Uc, dir_ptr: int, index: int, out_ptr: int) -> int:
        if out_ptr == 0:
            return 0
        path = self._read_guest_cstring(uc, dir_ptr)
        host_dir = self._guest_to_host(path, must_exist=True)
        if host_dir is None or not host_dir.is_dir():
            return 0

        children = self._list_children(host_dir)
        if index >= len(children):
            return 0

        name = children[int(index)]
        encoded = name.encode("utf-8", errors="replace")
        if len(encoded) >= FS_NAME_MAX:
            encoded = encoded[: FS_NAME_MAX - 1]

        return 1 if self._write_guest_bytes(uc, out_ptr, encoded + b"\x00") else 0

    def _fs_read(self, uc: Uc, path_ptr: int, out_ptr: int, buf_size: int) -> int:
        if out_ptr == 0 or buf_size == 0:
            return 0

        path = self._read_guest_cstring(uc, path_ptr)
        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None or not host_path.is_file():
            return 0

        read_size = int(min(buf_size, MAX_IO_READ))
        try:
            data = host_path.read_bytes()[:read_size]
        except Exception:
            return 0

        if not data:
            return 0
        return len(data) if self._write_guest_bytes(uc, out_ptr, data) else 0

    def _exec_path(self, uc: Uc, path_ptr: int) -> int:
        path = self._read_guest_cstring(uc, path_ptr)
        guest_path = self._normalize_guest_path(path)
        host_path = self._guest_to_host(guest_path, must_exist=True)

        self.state.exec_requests = u64(self.state.exec_requests + 1)
        self.state.user_exec_requested = 1
        self.state.user_launch_tries = u64(self.state.user_launch_tries + 1)

        if host_path is None or not host_path.is_file():
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        if self.depth >= self.max_exec_depth:
            print(f"[WINE][WARN] exec depth exceeded: {guest_path}", file=sys.stderr)
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        child = CLeonOSWineNative(
            elf_path=host_path,
            rootfs=self.rootfs,
            guest_path_hint=guest_path,
            state=self.state,
            depth=self.depth + 1,
            max_exec_depth=self.max_exec_depth,
            no_kbd=True,
            verbose=self.verbose,
            top_level=False,
        )
        child_ret = child.run()
        if child_ret is None:
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        self.state.exec_success = u64(self.state.exec_success + 1)
        self.state.user_launch_ok = u64(self.state.user_launch_ok + 1)
        if guest_path.lower().startswith("/system/"):
            self.state.kelf_runs = u64(self.state.kelf_runs + 1)
        return 0

    def _guest_to_host(self, guest_path: str, *, must_exist: bool) -> Optional[Path]:
        norm = self._normalize_guest_path(guest_path)
        if norm == "/":
            return self.rootfs if (not must_exist or self.rootfs.exists()) else None

        current = self.rootfs
        for part in [p for p in norm.split("/") if p]:
            candidate = current / part
            if candidate.exists():
                current = candidate
                continue

            if current.exists() and current.is_dir():
                match = self._find_case_insensitive(current, part)
                if match is not None:
                    current = match
                    continue

            current = candidate

        if must_exist and not current.exists():
            return None
        return current

    @staticmethod
    def _find_case_insensitive(parent: Path, name: str) -> Optional[Path]:
        target = name.lower()
        try:
            for entry in parent.iterdir():
                if entry.name.lower() == target:
                    return entry
        except Exception:
            return None
        return None

    @staticmethod
    def _normalize_guest_path(path: str) -> str:
        p = (path or "").replace("\\", "/").strip()
        if not p:
            return "/"
        if not p.startswith("/"):
            p = "/" + p

        parts = []
        for token in p.split("/"):
            if token in ("", "."):
                continue
            if token == "..":
                if parts:
                    parts.pop()
                continue
            parts.append(token)

        return "/" + "/".join(parts)

    @staticmethod
    def _list_children(dir_path: Path) -> List[str]:
        try:
            names = [entry.name for entry in dir_path.iterdir() if not entry.name.startswith(".")]
        except Exception:
            return []
        names.sort(key=lambda x: x.lower())
        return names


def resolve_rootfs(path_arg: Optional[str]) -> Path:
    if path_arg:
        root = Path(path_arg).expanduser().resolve()
        if not root.exists() or not root.is_dir():
            raise FileNotFoundError(f"rootfs not found: {root}")
        return root

    candidates = [
        Path("build/x86_64/ramdisk_root"),
        Path("ramdisk"),
    ]
    for candidate in candidates:
        if candidate.exists() and candidate.is_dir():
            return candidate.resolve()

    raise FileNotFoundError("rootfs not found; pass --rootfs")


def _guest_to_host_for_resolve(rootfs: Path, guest_path: str) -> Optional[Path]:
    norm = CLeonOSWineNative._normalize_guest_path(guest_path)
    if norm == "/":
        return rootfs

    current = rootfs
    for part in [p for p in norm.split("/") if p]:
        candidate = current / part
        if candidate.exists():
            current = candidate
            continue

        if current.exists() and current.is_dir():
            match = None
            for entry in current.iterdir():
                if entry.name.lower() == part.lower():
                    match = entry
                    break
            if match is not None:
                current = match
                continue

        current = candidate

    return current if current.exists() else None


def resolve_elf_target(elf_arg: str, rootfs: Path) -> Tuple[Path, str]:
    host_candidate = Path(elf_arg).expanduser()
    if host_candidate.exists():
        host_path = host_candidate.resolve()
        try:
            rel = host_path.relative_to(rootfs)
            guest_path = "/" + rel.as_posix()
        except ValueError:
            guest_path = "/" + host_path.name
        return host_path, guest_path

    guest_path = CLeonOSWineNative._normalize_guest_path(elf_arg)
    host_path = _guest_to_host_for_resolve(rootfs, guest_path)
    if host_path is None:
        raise FileNotFoundError(f"ELF not found as host path or guest path: {elf_arg}")
    return host_path.resolve(), guest_path


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
        print(f"[WINE] backend=unicorn", file=sys.stderr)
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


if __name__ == "__main__":
    raise SystemExit(main())
