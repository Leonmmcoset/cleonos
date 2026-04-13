from __future__ import annotations

import os
import sys
import threading
import time
from typing import Optional

from .state import SharedKernelState


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