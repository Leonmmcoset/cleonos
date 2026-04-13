from __future__ import annotations

import os
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

from .constants import (
    DEFAULT_MAX_EXEC_DEPTH,
    FS_NAME_MAX,
    MAX_CSTR,
    MAX_IO_READ,
    PAGE_SIZE,
    SYS_CONTEXT_SWITCHES,
    SYS_CUR_TASK,
    SYS_EXEC_PATH,
    SYS_EXEC_REQUESTS,
    SYS_EXEC_SUCCESS,
    SYS_EXIT,
    SYS_GETPID,
    SYS_SLEEP_TICKS,
    SYS_SPAWN_PATH,
    SYS_WAITPID,
    SYS_YIELD,
    SYS_FS_APPEND,
    SYS_FS_CHILD_COUNT,
    SYS_FS_GET_CHILD_NAME,
    SYS_FS_MKDIR,
    SYS_FS_NODE_COUNT,
    SYS_FS_READ,
    SYS_FS_REMOVE,
    SYS_FS_STAT_SIZE,
    SYS_FS_STAT_TYPE,
    SYS_FS_WRITE,
    SYS_KBD_BUFFERED,
    SYS_KBD_DROPPED,
    SYS_KBD_GET_CHAR,
    SYS_KBD_HOTKEY_SWITCHES,
    SYS_KBD_POPPED,
    SYS_KBD_PUSHED,
    SYS_KELF_COUNT,
    SYS_KELF_RUNS,
    SYS_LOG_JOURNAL_COUNT,
    SYS_LOG_JOURNAL_READ,
    SYS_LOG_WRITE,
    SYS_SERVICE_COUNT,
    SYS_SERVICE_READY_COUNT,
    SYS_TASK_COUNT,
    SYS_TIMER_TICKS,
    SYS_TTY_ACTIVE,
    SYS_TTY_COUNT,
    SYS_TTY_SWITCH,
    SYS_TTY_WRITE,
    SYS_TTY_WRITE_CHAR,
    SYS_USER_EXEC_REQUESTED,
    SYS_USER_LAUNCH_FAIL,
    SYS_USER_LAUNCH_OK,
    SYS_USER_LAUNCH_TRIES,
    SYS_USER_SHELL_READY,
    page_ceil,
    page_floor,
    u64,
    u64_neg1,
)
from .input_pump import InputPump
from .platform import (
    Uc,
    UcError,
    UC_ARCH_X86,
    UC_HOOK_CODE,
    UC_HOOK_INTR,
    UC_MODE_64,
    UC_PROT_ALL,
    UC_PROT_EXEC,
    UC_PROT_READ,
    UC_PROT_WRITE,
    UC_X86_REG_RAX,
    UC_X86_REG_RBP,
    UC_X86_REG_RBX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDX,
    UC_X86_REG_RSP,
)
from .state import SharedKernelState


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
        pid: int = 0,
        ppid: int = 0,
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
        self.pid = int(pid)
        self.ppid = int(ppid)
        self._exit_requested = False
        self._exit_status = 0

        self.image = self._parse_elf(self.elf_path)
        self.exit_code: Optional[int] = None
        self._input_pump: Optional[InputPump] = None

        self._stack_base = 0x00007FFF00000000
        self._stack_size = 0x0000000000020000
        self._ret_sentinel = 0x00007FFF10000000
        self._mapped_ranges: List[Tuple[int, int]] = []

    def run(self) -> Optional[int]:
        if self.pid == 0:
            self.pid = self.state.alloc_pid(self.ppid)

        prev_pid = self.state.get_current_pid()
        self.state.set_current_pid(self.pid)

        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self._install_hooks(uc)
        self._load_segments(uc)
        self._prepare_stack_and_return(uc)

        if self.top_level and not self.no_kbd:
            self._input_pump = InputPump(self.state)
            self._input_pump.start()

        run_failed = False

        try:
            uc.emu_start(self.image.entry, 0)
        except KeyboardInterrupt:
            run_failed = True
            if self.top_level:
                print("\n[WINE] interrupted by user", file=sys.stderr)
        except UcError as exc:
            run_failed = True
            if self.verbose or self.top_level:
                print(f"[WINE][ERROR] runtime crashed: {exc}", file=sys.stderr)
        finally:
            if self.top_level and self._input_pump is not None:
                self._input_pump.stop()

        if run_failed:
            self.state.mark_exited(self.pid, u64_neg1())
            self.state.set_current_pid(prev_pid)
            return None

        if self.exit_code is None:
            self.exit_code = self._reg_read(uc, UC_X86_REG_RAX)

        if self._exit_requested:
            self.exit_code = self._exit_status

        self.exit_code = u64(self.exit_code)
        self.state.mark_exited(self.pid, self.exit_code)
        self.state.set_current_pid(prev_pid)
        return self.exit_code

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
            text = data.decode("utf-8", errors="replace")
            self._host_write(text)
            self.state.log_journal_push(text)
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
        if sid == SYS_SPAWN_PATH:
            return self._spawn_path(uc, arg0)
        if sid == SYS_WAITPID:
            return self._wait_pid(uc, arg0, arg1)
        if sid == SYS_GETPID:
            return self.state.get_current_pid()
        if sid == SYS_EXIT:
            return self._request_exit(uc, arg0)
        if sid == SYS_SLEEP_TICKS:
            return self._sleep_ticks(arg0)
        if sid == SYS_YIELD:
            return self._yield_once()
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
            return self.state.tty_active
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
            return 1
        if sid == SYS_KBD_GET_CHAR:
            key = self.state.pop_key()
            return u64_neg1() if key is None else key
        if sid == SYS_FS_STAT_TYPE:
            return self._fs_stat_type(uc, arg0)
        if sid == SYS_FS_STAT_SIZE:
            return self._fs_stat_size(uc, arg0)
        if sid == SYS_FS_MKDIR:
            return self._fs_mkdir(uc, arg0)
        if sid == SYS_FS_WRITE:
            return self._fs_write(uc, arg0, arg1, arg2)
        if sid == SYS_FS_APPEND:
            return self._fs_append(uc, arg0, arg1, arg2)
        if sid == SYS_FS_REMOVE:
            return self._fs_remove(uc, arg0)
        if sid == SYS_LOG_JOURNAL_COUNT:
            return self.state.log_journal_count()
        if sid == SYS_LOG_JOURNAL_READ:
            return self._log_journal_read(uc, arg0, arg1, arg2)
        if sid == SYS_KBD_BUFFERED:
            return self.state.buffered_count()
        if sid == SYS_KBD_PUSHED:
            return self.state.kbd_push_count
        if sid == SYS_KBD_POPPED:
            return self.state.kbd_pop_count
        if sid == SYS_KBD_DROPPED:
            return self.state.kbd_drop_count
        if sid == SYS_KBD_HOTKEY_SWITCHES:
            return self.state.kbd_hotkey_switches

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

    def _fs_stat_type(self, uc: Uc, path_ptr: int) -> int:
        path = self._read_guest_cstring(uc, path_ptr)
        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None:
            return u64_neg1()
        if host_path.is_dir():
            return 2
        if host_path.is_file():
            return 1
        return u64_neg1()

    def _fs_stat_size(self, uc: Uc, path_ptr: int) -> int:
        path = self._read_guest_cstring(uc, path_ptr)
        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None:
            return u64_neg1()
        if host_path.is_dir():
            return 0
        if host_path.is_file():
            try:
                return host_path.stat().st_size
            except Exception:
                return u64_neg1()
        return u64_neg1()

    @staticmethod
    def _guest_path_is_under_temp(path: str) -> bool:
        return path == "/temp" or path.startswith("/temp/")

    def _fs_mkdir(self, uc: Uc, path_ptr: int) -> int:
        path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr))
        if not self._guest_path_is_under_temp(path):
            return 0

        host_path = self._guest_to_host(path, must_exist=False)
        if host_path is None:
            return 0

        if host_path.exists() and host_path.is_file():
            return 0

        try:
            host_path.mkdir(parents=True, exist_ok=True)
            return 1
        except Exception:
            return 0

    def _fs_write_common(self, uc: Uc, path_ptr: int, data_ptr: int, size: int, append_mode: bool) -> int:
        path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr))

        if not self._guest_path_is_under_temp(path) or path == "/temp":
            return 0

        if size < 0 or size > self.state.fs_write_max:
            return 0

        host_path = self._guest_to_host(path, must_exist=False)
        if host_path is None:
            return 0

        if host_path.exists() and host_path.is_dir():
            return 0

        data = b""
        if size > 0:
            if data_ptr == 0:
                return 0
            data = self._read_guest_bytes(uc, data_ptr, size)
            if len(data) != int(size):
                return 0

        try:
            host_path.parent.mkdir(parents=True, exist_ok=True)
            mode = "ab" if append_mode else "wb"
            with host_path.open(mode) as fh:
                if data:
                    fh.write(data)
            return 1
        except Exception:
            return 0

    def _fs_write(self, uc: Uc, path_ptr: int, data_ptr: int, size: int) -> int:
        return self._fs_write_common(uc, path_ptr, data_ptr, size, append_mode=False)

    def _fs_append(self, uc: Uc, path_ptr: int, data_ptr: int, size: int) -> int:
        return self._fs_write_common(uc, path_ptr, data_ptr, size, append_mode=True)

    def _fs_remove(self, uc: Uc, path_ptr: int) -> int:
        path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr))

        if not self._guest_path_is_under_temp(path) or path == "/temp":
            return 0

        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None:
            return 0

        try:
            if host_path.is_dir():
                if any(host_path.iterdir()):
                    return 0
                host_path.rmdir()
            else:
                host_path.unlink()
            return 1
        except Exception:
            return 0

    def _log_journal_read(self, uc: Uc, index_from_oldest: int, out_ptr: int, out_size: int) -> int:
        if out_ptr == 0 or out_size == 0:
            return 0

        line = self.state.log_journal_read(int(index_from_oldest))
        if line is None:
            return 0

        encoded = line.encode("utf-8", errors="replace")
        max_payload = int(out_size) - 1
        if max_payload < 0:
            return 0

        if len(encoded) > max_payload:
            encoded = encoded[:max_payload]

        return 1 if self._write_guest_bytes(uc, out_ptr, encoded + b"\x00") else 0

    def _exec_path(self, uc: Uc, path_ptr: int) -> int:
        return self._spawn_path_common(uc, path_ptr, return_pid=False)

    def _spawn_path(self, uc: Uc, path_ptr: int) -> int:
        return self._spawn_path_common(uc, path_ptr, return_pid=True)

    def _spawn_path_common(self, uc: Uc, path_ptr: int, *, return_pid: bool) -> int:
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

        parent_pid = self.state.get_current_pid()
        child_pid = self.state.alloc_pid(parent_pid)

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
            pid=child_pid,
            ppid=parent_pid,
        )
        child_ret = child.run()

        if child_ret is None:
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        self.state.exec_success = u64(self.state.exec_success + 1)
        self.state.user_launch_ok = u64(self.state.user_launch_ok + 1)

        if guest_path.lower().startswith("/system/"):
            self.state.kelf_runs = u64(self.state.kelf_runs + 1)

        if return_pid:
            return child_pid

        return 0

    def _wait_pid(self, uc: Uc, pid: int, out_ptr: int) -> int:
        wait_ret, status = self.state.wait_pid(int(pid))

        if wait_ret == 1 and out_ptr != 0:
            self._write_guest_bytes(uc, out_ptr, struct.pack("<Q", u64(status)))

        return int(wait_ret)

    def _request_exit(self, uc: Uc, status: int) -> int:
        self._exit_requested = True
        self._exit_status = u64(status)
        uc.emu_stop()
        return 1

    def _sleep_ticks(self, ticks: int) -> int:
        ticks = int(u64(ticks))

        if ticks == 0:
            return 0

        start = self.state.timer_ticks()

        while (self.state.timer_ticks() - start) < ticks:
            time.sleep(0.001)

        return self.state.timer_ticks() - start

    def _yield_once(self) -> int:
        time.sleep(0)
        return self.state.timer_ticks()

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
    return host_path.resolve(), guest_path`n