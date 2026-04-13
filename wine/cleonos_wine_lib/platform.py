from __future__ import annotations

import sys

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


__all__ = [
    "Uc",
    "UcError",
    "UC_ARCH_X86",
    "UC_MODE_64",
    "UC_HOOK_CODE",
    "UC_HOOK_INTR",
    "UC_PROT_ALL",
    "UC_PROT_EXEC",
    "UC_PROT_READ",
    "UC_PROT_WRITE",
    "UC_X86_REG_RAX",
    "UC_X86_REG_RBX",
    "UC_X86_REG_RCX",
    "UC_X86_REG_RDX",
    "UC_X86_REG_RBP",
    "UC_X86_REG_RSP",
]