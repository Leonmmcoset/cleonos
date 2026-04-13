from __future__ import annotations

import collections
import threading
import time
from dataclasses import dataclass, field
from typing import Deque, Dict, Optional, Tuple

from .constants import u64


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
    kbd_queue_cap: int = 256
    kbd_drop_count: int = 0
    kbd_push_count: int = 0
    kbd_pop_count: int = 0
    kbd_hotkey_switches: int = 0
    log_journal_cap: int = 256
    log_journal: Deque[str] = field(default_factory=lambda: collections.deque(maxlen=256))
    fs_write_max: int = 65536

    proc_lock: threading.Lock = field(default_factory=threading.Lock)
    proc_next_pid: int = 1
    proc_current_pid: int = 0
    proc_parents: Dict[int, int] = field(default_factory=dict)
    proc_status: Dict[int, Optional[int]] = field(default_factory=dict)

    def timer_ticks(self) -> int:
        return (time.monotonic_ns() - self.start_ns) // 1_000_000

    def push_key(self, key: int) -> None:
        with self.kbd_lock:
            if len(self.kbd_queue) >= self.kbd_queue_cap:
                self.kbd_queue.popleft()
                self.kbd_drop_count = u64(self.kbd_drop_count + 1)
            self.kbd_queue.append(key & 0xFF)
            self.kbd_push_count = u64(self.kbd_push_count + 1)

    def pop_key(self) -> Optional[int]:
        with self.kbd_lock:
            if not self.kbd_queue:
                return None
            self.kbd_pop_count = u64(self.kbd_pop_count + 1)
            return self.kbd_queue.popleft()

    def buffered_count(self) -> int:
        with self.kbd_lock:
            return len(self.kbd_queue)

    def log_journal_push(self, text: str) -> None:
        if text is None:
            return

        normalized = text.replace("\r", "")
        lines = normalized.split("\n")

        for line in lines:
            if len(line) > 255:
                line = line[:255]
            self.log_journal.append(line)

    def log_journal_count(self) -> int:
        return len(self.log_journal)

    def log_journal_read(self, index_from_oldest: int) -> Optional[str]:
        if index_from_oldest < 0 or index_from_oldest >= len(self.log_journal):
            return None
        return list(self.log_journal)[index_from_oldest]

    def alloc_pid(self, ppid: int) -> int:
        with self.proc_lock:
            pid = int(self.proc_next_pid)

            if pid == 0:
                pid = 1

            self.proc_next_pid = int(u64(pid + 1))

            if self.proc_next_pid == 0:
                self.proc_next_pid = 1

            self.proc_parents[pid] = int(ppid)
            self.proc_status[pid] = None
            return pid

    def set_current_pid(self, pid: int) -> None:
        with self.proc_lock:
            self.proc_current_pid = int(pid)

    def get_current_pid(self) -> int:
        with self.proc_lock:
            return int(self.proc_current_pid)

    def mark_exited(self, pid: int, status: int) -> None:
        if pid <= 0:
            return

        with self.proc_lock:
            self.proc_status[int(pid)] = int(u64(status))

    def wait_pid(self, pid: int) -> Tuple[int, int]:
        with self.proc_lock:
            if pid not in self.proc_status:
                return int(u64(-1)), 0

            status = self.proc_status[pid]

            if status is None:
                return 0, 0

            return 1, int(status)`n