#!/usr/bin/env python3
"""
CLeonOS menuconfig

Interactive feature selector that writes:
  - configs/menuconfig/.config.json
  - configs/menuconfig/config.cmake

Design:
  - CLKS options come from configs/menuconfig/clks_features.json
  - User-space app options are discovered dynamically from *_main.c / *_kmain.c
"""

from __future__ import annotations

import argparse
import os
import json
import re
import sys
import textwrap
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

try:
    import curses
except Exception:
    curses = None


ROOT_DIR = Path(__file__).resolve().parent.parent
APPS_DIR = ROOT_DIR / "cleonos" / "c" / "apps"
MENUCONFIG_DIR = ROOT_DIR / "configs" / "menuconfig"
CLKS_FEATURES_PATH = MENUCONFIG_DIR / "clks_features.json"
CONFIG_JSON_PATH = MENUCONFIG_DIR / ".config.json"
CONFIG_CMAKE_PATH = MENUCONFIG_DIR / "config.cmake"


@dataclass(frozen=True)
class OptionItem:
    key: str
    title: str
    description: str
    default: bool


def normalize_bool(raw: object, default: bool) -> bool:
    if isinstance(raw, bool):
        return raw
    if isinstance(raw, (int, float)):
        return raw != 0
    if isinstance(raw, str):
        text = raw.strip().lower()
        if text in {"1", "on", "true", "yes", "y"}:
            return True
        if text in {"0", "off", "false", "no", "n"}:
            return False
    return default


def sanitize_token(name: str) -> str:
    token = re.sub(r"[^A-Za-z0-9]+", "_", name.strip().upper())
    token = token.strip("_")
    return token or "UNKNOWN"


def load_clks_options() -> List[OptionItem]:
    if not CLKS_FEATURES_PATH.exists():
        raise RuntimeError(f"missing CLKS feature file: {CLKS_FEATURES_PATH}")

    raw = json.loads(CLKS_FEATURES_PATH.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or "features" not in raw or not isinstance(raw["features"], list):
        raise RuntimeError(f"invalid feature format in {CLKS_FEATURES_PATH}")

    options: List[OptionItem] = []
    for entry in raw["features"]:
        if not isinstance(entry, dict):
            continue
        key = str(entry.get("key", "")).strip()
        title = str(entry.get("title", key)).strip()
        description = str(entry.get("description", "")).strip()
        default = normalize_bool(entry.get("default", True), True)
        if not key:
            continue
        options.append(OptionItem(key=key, title=title, description=description, default=default))

    if not options:
        raise RuntimeError(f"no CLKS feature options in {CLKS_FEATURES_PATH}")
    return options


def discover_user_apps() -> List[OptionItem]:
    main_paths = sorted(APPS_DIR.glob("*_main.c"))
    kmain_paths = sorted(APPS_DIR.glob("*_kmain.c"))

    kmain_names = set()
    for path in kmain_paths:
        name = path.stem
        if name.endswith("_kmain"):
            kmain_names.add(name[:-6])

    final_apps: List[Tuple[str, str]] = []
    for path in main_paths:
        name = path.stem
        if not name.endswith("_main"):
            continue
        app = name[:-5]
        if app in kmain_names:
            continue
        if app.endswith("drv"):
            section = "driver"
        elif app == "hello":
            section = "root"
        else:
            section = "shell"
        final_apps.append((app, section))

    for app in sorted(kmain_names):
        final_apps.append((app, "system"))

    final_apps.sort(key=lambda item: (item[1], item[0]))

    options: List[OptionItem] = []
    for app, section in final_apps:
        key = f"CLEONOS_USER_APP_{sanitize_token(app)}"
        title = f"{app}.elf [{section}]"
        description = f"Build and package user app '{app}' into ramdisk/{section}."
        options.append(OptionItem(key=key, title=title, description=description, default=True))

    return options


def load_previous_values() -> Dict[str, bool]:
    if not CONFIG_JSON_PATH.exists():
        return {}
    try:
        raw = json.loads(CONFIG_JSON_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {}

    if not isinstance(raw, dict):
        return {}

    out: Dict[str, bool] = {}
    for key, value in raw.items():
        if not isinstance(key, str):
            continue
        out[key] = normalize_bool(value, False)
    return out


def init_values(options: Iterable[OptionItem], previous: Dict[str, bool], use_defaults: bool) -> Dict[str, bool]:
    values: Dict[str, bool] = {}
    for item in options:
        if not use_defaults and item.key in previous:
            values[item.key] = previous[item.key]
        else:
            values[item.key] = item.default
    return values


def print_section(title: str, options: List[OptionItem], values: Dict[str, bool]) -> None:
    print()
    print(f"== {title} ==")
    for idx, item in enumerate(options, start=1):
        mark = "x" if values.get(item.key, item.default) else " "
        print(f"{idx:3d}. [{mark}] {item.title}")
    print("Commands: <number> toggle, a enable-all, n disable-all, i <n> info, b back")


def section_loop(title: str, options: List[OptionItem], values: Dict[str, bool]) -> None:
    while True:
        print_section(title, options, values)
        raw = input(f"{title}> ").strip()
        if not raw:
            continue
        lower = raw.lower()

        if lower in {"b", "back", "q", "quit"}:
            return
        if lower in {"a", "all", "on"}:
            for item in options:
                values[item.key] = True
            continue
        if lower in {"n", "none", "off"}:
            for item in options:
                values[item.key] = False
            continue
        if lower.startswith("i "):
            token = lower[2:].strip()
            if token.isdigit():
                idx = int(token)
                if 1 <= idx <= len(options):
                    item = options[idx - 1]
                    state = "ON" if values.get(item.key, item.default) else "OFF"
                    print()
                    print(f"[{idx}] {item.title}")
                    print(f"key: {item.key}")
                    print(f"state: {state}")
                    print(f"desc: {item.description}")
                    continue
            print("invalid info index")
            continue

        if raw.isdigit():
            idx = int(raw)
            if 1 <= idx <= len(options):
                item = options[idx - 1]
                values[item.key] = not values.get(item.key, item.default)
            else:
                print("invalid index")
            continue

        print("unknown command")


def _safe_addnstr(stdscr, y: int, x: int, text: str, attr: int = 0) -> None:
    h, w = stdscr.getmaxyx()
    if y < 0 or y >= h or x >= w:
        return
    max_len = max(0, w - x - 1)
    if max_len <= 0:
        return
    try:
        stdscr.addnstr(y, x, text, max_len, attr)
    except Exception:
        pass


def _option_enabled(values: Dict[str, bool], item: OptionItem) -> bool:
    return values.get(item.key, item.default)


def _set_all(values: Dict[str, bool], options: List[OptionItem], enabled: bool) -> None:
    for item in options:
        values[item.key] = enabled


def _run_ncurses_section(stdscr, title: str, options: List[OptionItem], values: Dict[str, bool]) -> None:
    selected = 0
    top = 0

    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()

        if h < 10 or w < 40:
            _safe_addnstr(stdscr, 0, 0, "Terminal too small for menuconfig (need >= 40x10).", curses.A_BOLD)
            _safe_addnstr(stdscr, 2, 0, "Resize terminal then press any key, or ESC to go back.")
            key = stdscr.getch()
            if key in (27,):
                return
            continue

        list_top = 2
        desc_area = 4
        help_area = 2
        list_bottom = h - (desc_area + help_area) - 1
        visible = max(1, list_bottom - list_top + 1)

        if selected < 0:
            selected = 0
        if selected >= len(options):
            selected = max(0, len(options) - 1)

        if selected < top:
            top = selected
        if selected >= top + visible:
            top = selected - visible + 1
        if top < 0:
            top = 0

        _safe_addnstr(stdscr, 0, 0, f"CLeonOS menuconfig / {title}", curses.A_REVERSE)
        _safe_addnstr(stdscr, 1, 0, f"Items: {len(options)}")

        for row in range(visible):
            idx = top + row
            if idx >= len(options):
                break
            item = options[idx]
            mark = "x" if _option_enabled(values, item) else " "
            line = f"{idx + 1:3d}. [{mark}] {item.title}"
            attr = curses.A_REVERSE if idx == selected else 0
            _safe_addnstr(stdscr, list_top + row, 0, line, attr)

        if options:
            cur = options[selected]
            key_line = f"{cur.key}"
            desc_line = cur.description
            wrapped = textwrap.wrap(desc_line, max(10, w - 2))
            _safe_addnstr(stdscr, list_bottom + 1, 0, key_line, curses.A_DIM)
            for i, part in enumerate(wrapped[: max(1, desc_area - 1)]):
                _safe_addnstr(stdscr, list_bottom + 2 + i, 0, part)

        _safe_addnstr(
            stdscr,
            h - 2,
            0,
            "Arrows/jk move  Space toggle  a all-on  n all-off  PgUp/PgDn  Home/End",
            curses.A_DIM,
        )
        _safe_addnstr(stdscr, h - 1, 0, "Enter/ESC/q back", curses.A_DIM)

        stdscr.refresh()
        key = stdscr.getch()

        if key in (27, ord("q"), ord("Q"), curses.KEY_LEFT, curses.KEY_ENTER, 10, 13):
            return
        if key in (curses.KEY_UP, ord("k"), ord("K")):
            selected -= 1
            continue
        if key in (curses.KEY_DOWN, ord("j"), ord("J")):
            selected += 1
            continue
        if key == curses.KEY_PPAGE:
            selected -= visible
            continue
        if key == curses.KEY_NPAGE:
            selected += visible
            continue
        if key == curses.KEY_HOME:
            selected = 0
            continue
        if key == curses.KEY_END:
            selected = max(0, len(options) - 1)
            continue
        if key == ord(" "):
            if options:
                item = options[selected]
                values[item.key] = not _option_enabled(values, item)
            continue
        if key in (ord("a"), ord("A")):
            _set_all(values, options, True)
            continue
        if key in (ord("n"), ord("N")):
            _set_all(values, options, False)
            continue


def _run_ncurses_main(stdscr, clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, bool]) -> bool:
    try:
        curses.curs_set(0)
    except Exception:
        pass
    stdscr.keypad(True)
    selected = 0

    while True:
        stdscr.erase()
        h, _w = stdscr.getmaxyx()

        clks_on = sum(1 for item in clks_options if _option_enabled(values, item))
        user_on = sum(1 for item in user_options if _option_enabled(values, item))

        items = [
            f"CLKS features ({clks_on}/{len(clks_options)} enabled)",
            f"User apps ({user_on}/{len(user_options)} enabled)",
            "Save and Exit",
            "Quit without Saving",
        ]

        _safe_addnstr(stdscr, 0, 0, "CLeonOS menuconfig (ncurses)", curses.A_REVERSE)
        _safe_addnstr(stdscr, 1, 0, "Enter open/select, Arrow/jk move, s save, q quit", curses.A_DIM)

        base = 3
        for i, text in enumerate(items):
            attr = curses.A_REVERSE if i == selected else 0
            _safe_addnstr(stdscr, base + i, 0, text, attr)

        _safe_addnstr(stdscr, h - 1, 0, "Tip: Space toggles options inside sections.", curses.A_DIM)
        stdscr.refresh()

        key = stdscr.getch()

        if key in (ord("q"), ord("Q"), 27):
            return False
        if key in (ord("s"), ord("S")):
            return True
        if key in (curses.KEY_UP, ord("k"), ord("K")):
            selected = (selected - 1) % len(items)
            continue
        if key in (curses.KEY_DOWN, ord("j"), ord("J")):
            selected = (selected + 1) % len(items)
            continue
        if key in (curses.KEY_ENTER, 10, 13):
            if selected == 0:
                _run_ncurses_section(stdscr, "CLKS", clks_options, values)
            elif selected == 1:
                _run_ncurses_section(stdscr, "USER", user_options, values)
            elif selected == 2:
                return True
            else:
                return False
            continue


def interactive_menu_ncurses(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, bool]) -> bool:
    if curses is None:
        raise RuntimeError("python curses module unavailable (install python3-curses / ncurses)")
    if "TERM" not in os.environ or not os.environ["TERM"]:
        raise RuntimeError("TERM is not set; cannot start ncurses UI")
    return bool(curses.wrapper(lambda stdscr: _run_ncurses_main(stdscr, clks_options, user_options, values)))


def write_outputs(all_values: Dict[str, bool], ordered_options: List[OptionItem]) -> None:
    MENUCONFIG_DIR.mkdir(parents=True, exist_ok=True)

    ordered_keys = [item.key for item in ordered_options]
    output_values: Dict[str, bool] = {key: all_values[key] for key in ordered_keys if key in all_values}

    CONFIG_JSON_PATH.write_text(
        json.dumps(output_values, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    lines = [
        "# Auto-generated by scripts/menuconfig.py",
        "# Do not edit manually unless you know what you are doing.",
        'set(CLEONOS_MENUCONFIG_LOADED ON CACHE BOOL "CLeonOS menuconfig loaded" FORCE)',
    ]
    for item in ordered_options:
        value = "ON" if all_values.get(item.key, item.default) else "OFF"
        lines.append(f'set({item.key} {value} CACHE BOOL "{item.title}" FORCE)')

    CONFIG_CMAKE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def show_summary(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, bool]) -> None:
    clks_on = sum(1 for item in clks_options if values.get(item.key, item.default))
    user_on = sum(1 for item in user_options if values.get(item.key, item.default))
    print()
    print("========== CLeonOS menuconfig ==========")
    print(f"1) CLKS features : {clks_on}/{len(clks_options)} enabled")
    print(f"2) User features : {user_on}/{len(user_options)} enabled")
    print("s) Save and exit")
    print("q) Quit without saving")


def interactive_menu(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, bool]) -> bool:
    while True:
        show_summary(clks_options, user_options, values)
        choice = input("Select> ").strip().lower()
        if choice == "1":
            section_loop("CLKS", clks_options, values)
            continue
        if choice == "2":
            section_loop("USER", user_options, values)
            continue
        if choice in {"s", "save"}:
            return True
        if choice in {"q", "quit"}:
            return False
        print("unknown selection")


def parse_set_overrides(values: Dict[str, bool], kv_pairs: List[str]) -> None:
    for pair in kv_pairs:
        if "=" not in pair:
            raise RuntimeError(f"invalid --set entry: {pair!r}, expected KEY=ON|OFF")
        key, raw = pair.split("=", 1)
        key = key.strip()
        if not key:
            raise RuntimeError(f"invalid --set entry: {pair!r}, empty key")
        values[key] = normalize_bool(raw, False)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CLeonOS menuconfig")
    parser.add_argument("--defaults", action="store_true", help="ignore previous .config and use defaults")
    parser.add_argument("--non-interactive", action="store_true", help="save config without opening interactive menu")
    parser.add_argument("--plain", action="store_true", help="use legacy plain-text menu instead of ncurses")
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="KEY=ON|OFF",
        help="override one option before save (can be repeated)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    clks_options = load_clks_options()
    user_options = discover_user_apps()
    all_options = clks_options + user_options

    previous = load_previous_values()
    values = init_values(all_options, previous, use_defaults=args.defaults)
    parse_set_overrides(values, args.set)

    should_save = args.non_interactive
    if not args.non_interactive:
        if not sys.stdin.isatty():
            raise RuntimeError("menuconfig requires interactive tty (or use --non-interactive)")
        if args.plain:
            should_save = interactive_menu(clks_options, user_options, values)
        else:
            should_save = interactive_menu_ncurses(clks_options, user_options, values)

    if not should_save:
        print("menuconfig: no changes saved")
        return 0

    write_outputs(values, all_options)
    print(f"menuconfig: wrote {CONFIG_JSON_PATH}")
    print(f"menuconfig: wrote {CONFIG_CMAKE_PATH}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"menuconfig error: {exc}", file=sys.stderr)
        raise SystemExit(1)
