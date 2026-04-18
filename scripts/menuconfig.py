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

try:
    from PySide6 import QtCore, QtWidgets
except Exception:
    try:
        from PySide2 import QtCore, QtWidgets
    except Exception:
        QtCore = None
        QtWidgets = None


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


def _safe_addch(stdscr, y: int, x: int, ch, attr: int = 0) -> None:
    h, w = stdscr.getmaxyx()
    if y < 0 or y >= h or x < 0 or x >= w:
        return
    try:
        stdscr.addch(y, x, ch, attr)
    except Exception:
        pass


def _curses_theme() -> Dict[str, int]:
    # Reasonable monochrome fallback first.
    theme = {
        "header": curses.A_BOLD,
        "subtitle": curses.A_DIM,
        "panel_border": curses.A_DIM,
        "panel_title": curses.A_BOLD,
        "selected": curses.A_REVERSE | curses.A_BOLD,
        "enabled": curses.A_BOLD,
        "disabled": curses.A_DIM,
        "value_key": curses.A_DIM,
        "value_label": curses.A_BOLD,
        "help": curses.A_DIM,
        "status_ok": curses.A_BOLD,
        "status_warn": curses.A_BOLD,
        "progress_on": curses.A_REVERSE,
        "progress_off": curses.A_DIM,
        "scroll_track": curses.A_DIM,
        "scroll_thumb": curses.A_BOLD,
    }

    if not curses.has_colors():
        return theme

    try:
        curses.start_color()
    except Exception:
        return theme

    try:
        curses.use_default_colors()
    except Exception:
        pass

    # Pair index map
    # 1: Header
    # 2: Subtitle
    # 3: Panel border/title
    # 4: Selected row
    # 5: Enabled accent
    # 6: Disabled accent
    # 7: Footer/help
    # 8: Success/status
    # 9: Warning/status
    # 10: Scroll thumb
    try:
        curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(2, curses.COLOR_CYAN, -1)
        curses.init_pair(3, curses.COLOR_BLUE, -1)
        curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_BLUE)
        curses.init_pair(5, curses.COLOR_GREEN, -1)
        curses.init_pair(6, curses.COLOR_RED, -1)
        curses.init_pair(7, curses.COLOR_BLACK, curses.COLOR_WHITE)
        curses.init_pair(8, curses.COLOR_BLACK, curses.COLOR_GREEN)
        curses.init_pair(9, curses.COLOR_BLACK, curses.COLOR_YELLOW)
        curses.init_pair(10, curses.COLOR_MAGENTA, -1)
    except Exception:
        return theme

    theme.update(
        {
            "header": curses.color_pair(1) | curses.A_BOLD,
            "subtitle": curses.color_pair(2) | curses.A_DIM,
            "panel_border": curses.color_pair(3),
            "panel_title": curses.color_pair(3) | curses.A_BOLD,
            "selected": curses.color_pair(4) | curses.A_BOLD,
            "enabled": curses.color_pair(5) | curses.A_BOLD,
            "disabled": curses.color_pair(6) | curses.A_DIM,
            "value_key": curses.color_pair(2) | curses.A_DIM,
            "value_label": curses.A_BOLD,
            "help": curses.color_pair(7),
            "status_ok": curses.color_pair(8) | curses.A_BOLD,
            "status_warn": curses.color_pair(9) | curses.A_BOLD,
            "progress_on": curses.color_pair(5) | curses.A_REVERSE,
            "progress_off": curses.A_DIM,
            "scroll_track": curses.A_DIM,
            "scroll_thumb": curses.color_pair(10) | curses.A_BOLD,
        }
    )
    return theme


def _draw_box(stdscr, y: int, x: int, h: int, w: int, title: str, border_attr: int, title_attr: int) -> None:
    if h < 2 or w < 4:
        return
    right = x + w - 1
    bottom = y + h - 1

    _safe_addch(stdscr, y, x, curses.ACS_ULCORNER, border_attr)
    _safe_addch(stdscr, y, right, curses.ACS_URCORNER, border_attr)
    _safe_addch(stdscr, bottom, x, curses.ACS_LLCORNER, border_attr)
    _safe_addch(stdscr, bottom, right, curses.ACS_LRCORNER, border_attr)

    for col in range(x + 1, right):
        _safe_addch(stdscr, y, col, curses.ACS_HLINE, border_attr)
        _safe_addch(stdscr, bottom, col, curses.ACS_HLINE, border_attr)
    for row in range(y + 1, bottom):
        _safe_addch(stdscr, row, x, curses.ACS_VLINE, border_attr)
        _safe_addch(stdscr, row, right, curses.ACS_VLINE, border_attr)

    if title:
        _safe_addnstr(stdscr, y, x + 2, f" {title} ", title_attr)


def _draw_progress_bar(
    stdscr,
    y: int,
    x: int,
    width: int,
    enabled_count: int,
    total_count: int,
    on_attr: int,
    off_attr: int,
) -> None:
    if width < 8 or total_count <= 0:
        return
    bar_w = width - 8
    if bar_w < 4:
        return
    fill = int((enabled_count * bar_w) / total_count)
    for i in range(bar_w):
        ch = "#" if i < fill else "-"
        attr = on_attr if i < fill else off_attr
        _safe_addch(stdscr, y, x + i, ch, attr)
    _safe_addnstr(stdscr, y, x + bar_w + 1, f"{enabled_count:>3}/{total_count:<3}", off_attr | curses.A_BOLD)


def _option_enabled(values: Dict[str, bool], item: OptionItem) -> bool:
    return values.get(item.key, item.default)


def _set_all(values: Dict[str, bool], options: List[OptionItem], enabled: bool) -> None:
    for item in options:
        values[item.key] = enabled


def _draw_scrollbar(stdscr, y: int, x: int, height: int, total: int, top: int, visible: int, track_attr: int, thumb_attr: int) -> None:
    if height <= 0:
        return
    for r in range(height):
        _safe_addch(stdscr, y + r, x, "|", track_attr)

    if total <= 0 or visible <= 0 or total <= visible:
        for r in range(height):
            _safe_addch(stdscr, y + r, x, "|", thumb_attr)
        return

    thumb_h = max(1, int((visible * height) / total))
    if thumb_h > height:
        thumb_h = height

    max_top = max(1, total - visible)
    max_pos = max(0, height - thumb_h)
    thumb_y = int((top * max_pos) / max_top)

    for r in range(thumb_h):
        _safe_addch(stdscr, y + thumb_y + r, x, "#", thumb_attr)


def _run_ncurses_section(stdscr, theme: Dict[str, int], title: str, options: List[OptionItem], values: Dict[str, bool]) -> None:
    selected = 0
    top = 0

    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()

        if h < 14 or w < 70:
            _safe_addnstr(stdscr, 0, 0, "Terminal too small for rich UI (need >= 70x14).", theme["status_warn"])
            _safe_addnstr(stdscr, 2, 0, "Resize terminal then press any key, or ESC to go back.")
            key = stdscr.getch()
            if key in (27,):
                return
            continue

        left_w = max(38, int(w * 0.58))
        right_w = w - left_w
        if right_w < 24:
            left_w = w - 24
            right_w = 24

        list_box_y = 2
        list_box_x = 0
        list_box_h = h - 4
        list_box_w = left_w

        detail_box_y = 2
        detail_box_x = left_w
        detail_box_h = h - 4
        detail_box_w = w - left_w

        _safe_addnstr(stdscr, 0, 0, f" CLeonOS menuconfig / {title} ", theme["header"])
        enabled_count = sum(1 for item in options if _option_enabled(values, item))
        _safe_addnstr(
            stdscr,
            1,
            0,
            f" {enabled_count}/{len(options)} enabled  |  Arrow/jk move  Space toggle  a/n all  PgUp/PgDn  Enter/ESC back ",
            theme["subtitle"],
        )

        _draw_box(stdscr, list_box_y, list_box_x, list_box_h, list_box_w, "Options", theme["panel_border"], theme["panel_title"])
        _draw_box(stdscr, detail_box_y, detail_box_x, detail_box_h, detail_box_w, "Details", theme["panel_border"], theme["panel_title"])

        list_inner_y = list_box_y + 1
        list_inner_x = list_box_x + 1
        list_inner_h = list_box_h - 2
        list_inner_w = list_box_w - 2
        visible = max(1, list_inner_h)

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

        for row in range(visible):
            idx = top + row
            if idx >= len(options):
                break
            item = options[idx]
            enabled = _option_enabled(values, item)
            mark = "x" if enabled else " "
            prefix = ">" if idx == selected else " "
            line = f"{prefix} {idx + 1:03d} [{mark}] {item.title}"
            base_attr = theme["enabled"] if enabled else theme["disabled"]
            attr = theme["selected"] if idx == selected else base_attr
            _safe_addnstr(stdscr, list_inner_y + row, list_inner_x, line, attr)

        _draw_scrollbar(
            stdscr,
            list_inner_y,
            list_box_x + list_box_w - 2,
            list_inner_h,
            len(options),
            top,
            visible,
            theme["scroll_track"],
            theme["scroll_thumb"],
        )

        if options:
            cur = options[selected]
            detail_inner_y = detail_box_y + 1
            detail_inner_x = detail_box_x + 2
            detail_inner_w = detail_box_w - 4
            detail_inner_h = detail_box_h - 2

            state_text = "ENABLED" if _option_enabled(values, cur) else "DISABLED"
            state_attr = theme["status_ok"] if _option_enabled(values, cur) else theme["status_warn"]

            _safe_addnstr(stdscr, detail_inner_y + 0, detail_inner_x, cur.title, theme["value_label"])
            _safe_addnstr(stdscr, detail_inner_y + 1, detail_inner_x, cur.key, theme["value_key"])
            _safe_addnstr(stdscr, detail_inner_y + 2, detail_inner_x, f"State: {state_text}", state_attr)
            _safe_addnstr(
                stdscr,
                detail_inner_y + 3,
                detail_inner_x,
                f"Item: {selected + 1}/{len(options)}",
                theme["value_label"],
            )
            _draw_progress_bar(
                stdscr,
                detail_inner_y + 4,
                detail_inner_x,
                max(12, detail_inner_w),
                enabled_count,
                max(1, len(options)),
                theme["progress_on"],
                theme["progress_off"],
            )

            desc_title_y = detail_inner_y + 6
            _safe_addnstr(stdscr, desc_title_y, detail_inner_x, "Description:", theme["value_label"])
            wrapped = textwrap.wrap(cur.description, max(12, detail_inner_w))
            max_desc_lines = max(1, detail_inner_h - 8)
            for i, part in enumerate(wrapped[:max_desc_lines]):
                _safe_addnstr(stdscr, desc_title_y + 1 + i, detail_inner_x, part, 0)

        _safe_addnstr(stdscr, h - 1, 0, " Space:toggle  a:all-on  n:all-off  Enter/ESC:back ", theme["help"])

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
    theme = _curses_theme()
    try:
        curses.curs_set(0)
    except Exception:
        pass
    stdscr.keypad(True)
    selected = 0

    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()

        clks_on = sum(1 for item in clks_options if _option_enabled(values, item))
        user_on = sum(1 for item in user_options if _option_enabled(values, item))
        total_items = len(clks_options) + len(user_options)
        total_on = clks_on + user_on

        items = [
            f"CLKS features ({clks_on}/{len(clks_options)} enabled)",
            f"User apps ({user_on}/{len(user_options)} enabled)",
            "Save and Exit",
            "Quit without Saving",
        ]

        if h < 12 or w < 58:
            _safe_addnstr(stdscr, 0, 0, "Terminal too small for menuconfig (need >= 58x12).", theme["status_warn"])
            _safe_addnstr(stdscr, 2, 0, "Resize terminal then press any key.")
            stdscr.getch()
            continue

        _safe_addnstr(stdscr, 0, 0, " CLeonOS menuconfig ", theme["header"])
        _safe_addnstr(stdscr, 1, 0, " Stylish ncurses UI  |  Enter: open/select  s: save  q: quit ", theme["subtitle"])

        _draw_box(stdscr, 2, 0, h - 5, w, "Main", theme["panel_border"], theme["panel_title"])

        base = 4
        for i, text in enumerate(items):
            prefix = ">" if i == selected else " "
            row_text = f"{prefix} {text}"
            attr = theme["selected"] if i == selected else theme["value_label"]
            _safe_addnstr(stdscr, base + i, 2, row_text, attr)

        _safe_addnstr(stdscr, base + 6, 2, "Global Progress:", theme["value_label"])
        _draw_progress_bar(
            stdscr,
            base + 7,
            2,
            max(18, w - 6),
            total_on,
            max(1, total_items),
            theme["progress_on"],
            theme["progress_off"],
        )

        _safe_addnstr(stdscr, h - 2, 0, " Arrows/jk move  Enter select  s save  q quit ", theme["help"])
        _safe_addnstr(stdscr, h - 1, 0, " Tip: open CLKS/USER section then use Space to toggle options. ", theme["help"])
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
                _run_ncurses_section(stdscr, theme, "CLKS", clks_options, values)
            elif selected == 1:
                _run_ncurses_section(stdscr, theme, "USER", user_options, values)
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


def interactive_menu_gui(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, bool]) -> bool:
    if QtWidgets is None or QtCore is None:
        raise RuntimeError("python PySide unavailable (install PySide6, or use --plain)")

    if os.name != "nt" and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        raise RuntimeError("GUI mode requires a desktop display (DISPLAY/WAYLAND_DISPLAY)")

    app = QtWidgets.QApplication.instance()
    owns_app = False

    if app is None:
        app = QtWidgets.QApplication(["menuconfig-gui"])
        owns_app = True

    qt_checked = getattr(QtCore.Qt, "Checked", QtCore.Qt.CheckState.Checked)
    qt_unchecked = getattr(QtCore.Qt, "Unchecked", QtCore.Qt.CheckState.Unchecked)
    qt_horizontal = getattr(QtCore.Qt, "Horizontal", QtCore.Qt.Orientation.Horizontal)
    qt_item_enabled = getattr(QtCore.Qt, "ItemIsEnabled", QtCore.Qt.ItemFlag.ItemIsEnabled)
    qt_item_selectable = getattr(QtCore.Qt, "ItemIsSelectable", QtCore.Qt.ItemFlag.ItemIsSelectable)
    qt_item_checkable = getattr(QtCore.Qt, "ItemIsUserCheckable", QtCore.Qt.ItemFlag.ItemIsUserCheckable)

    resize_to_contents = getattr(
        QtWidgets.QHeaderView,
        "ResizeToContents",
        QtWidgets.QHeaderView.ResizeMode.ResizeToContents,
    )
    stretch_mode = getattr(
        QtWidgets.QHeaderView,
        "Stretch",
        QtWidgets.QHeaderView.ResizeMode.Stretch,
    )
    select_rows = getattr(
        QtWidgets.QAbstractItemView,
        "SelectRows",
        QtWidgets.QAbstractItemView.SelectionBehavior.SelectRows,
    )
    extended_selection = getattr(
        QtWidgets.QAbstractItemView,
        "ExtendedSelection",
        QtWidgets.QAbstractItemView.SelectionMode.ExtendedSelection,
    )

    result = {"save": False}

    dialog = QtWidgets.QDialog()
    dialog.setWindowTitle("CLeonOS menuconfig (PySide)")
    dialog.resize(1180, 760)
    dialog.setMinimumSize(920, 560)

    if os.name == "nt":
        dialog.setWindowState(dialog.windowState() | QtCore.Qt.WindowMaximized)

    root_layout = QtWidgets.QVBoxLayout(dialog)
    root_layout.setContentsMargins(12, 10, 12, 12)
    root_layout.setSpacing(8)

    header_title = QtWidgets.QLabel("CLeonOS menuconfig")
    header_font = header_title.font()
    header_font.setPointSize(header_font.pointSize() + 4)
    header_font.setBold(True)
    header_title.setFont(header_font)
    root_layout.addWidget(header_title)

    root_layout.addWidget(QtWidgets.QLabel("Window mode (PySide): configure CLKS features and user apps, then save."))

    summary_label = QtWidgets.QLabel("")
    root_layout.addWidget(summary_label)

    tabs = QtWidgets.QTabWidget()
    root_layout.addWidget(tabs, 1)

    def update_summary() -> None:
        clks_on = sum(1 for item in clks_options if values.get(item.key, item.default))
        user_on = sum(1 for item in user_options if values.get(item.key, item.default))
        total = len(clks_options) + len(user_options)
        summary_label.setText(
            f"CLKS: {clks_on}/{len(clks_options)} enabled    "
            f"User: {user_on}/{len(user_options)} enabled    "
            f"Total: {clks_on + user_on}/{total}"
        )

    class _SectionPanel(QtWidgets.QWidget):
        def __init__(self, title: str, options: List[OptionItem]):
            super().__init__()
            self.options = options
            self._updating = False

            layout = QtWidgets.QVBoxLayout(self)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(8)

            toolbar = QtWidgets.QHBoxLayout()
            title_label = QtWidgets.QLabel(title)
            title_font = title_label.font()
            title_font.setBold(True)
            title_label.setFont(title_font)
            toolbar.addWidget(title_label)
            toolbar.addStretch(1)

            toggle_btn = QtWidgets.QPushButton("Toggle Selected")
            enable_all_btn = QtWidgets.QPushButton("Enable All")
            disable_all_btn = QtWidgets.QPushButton("Disable All")
            toolbar.addWidget(enable_all_btn)
            toolbar.addWidget(disable_all_btn)
            toolbar.addWidget(toggle_btn)
            layout.addLayout(toolbar)

            splitter = QtWidgets.QSplitter(qt_horizontal)
            layout.addWidget(splitter, 1)

            left = QtWidgets.QWidget()
            left_layout = QtWidgets.QVBoxLayout(left)
            left_layout.setContentsMargins(0, 0, 0, 0)
            self.table = QtWidgets.QTableWidget(len(options), 2)
            self.table.setHorizontalHeaderLabels(["On", "Option"])
            self.table.verticalHeader().setVisible(False)
            self.table.horizontalHeader().setSectionResizeMode(0, resize_to_contents)
            self.table.horizontalHeader().setSectionResizeMode(1, stretch_mode)
            self.table.setSelectionBehavior(select_rows)
            self.table.setSelectionMode(extended_selection)
            self.table.setAlternatingRowColors(True)
            left_layout.addWidget(self.table)
            splitter.addWidget(left)

            right = QtWidgets.QWidget()
            right_layout = QtWidgets.QVBoxLayout(right)
            right_layout.setContentsMargins(0, 0, 0, 0)
            self.state_label = QtWidgets.QLabel("State: -")
            self.key_label = QtWidgets.QLabel("Key: -")
            self.detail_text = QtWidgets.QPlainTextEdit()
            self.detail_text.setReadOnly(True)
            right_layout.addWidget(self.state_label)
            right_layout.addWidget(self.key_label)
            right_layout.addWidget(self.detail_text, 1)
            splitter.addWidget(right)
            splitter.setStretchFactor(0, 3)
            splitter.setStretchFactor(1, 2)

            toggle_btn.clicked.connect(self.toggle_selected)
            enable_all_btn.clicked.connect(self.enable_all)
            disable_all_btn.clicked.connect(self.disable_all)
            self.table.itemSelectionChanged.connect(self._on_selection_changed)
            self.table.itemChanged.connect(self._on_item_changed)

            self.refresh(keep_selection=False)
            if self.options:
                self.table.selectRow(0)
                self._show_detail(0)

        def _selected_rows(self) -> List[int]:
            rows = []
            model = self.table.selectionModel()

            if model is None:
                return rows

            for idx in model.selectedRows():
                row = idx.row()
                if row not in rows:
                    rows.append(row)

            rows.sort()
            return rows

        def _show_detail(self, row: int) -> None:
            if row < 0 or row >= len(self.options):
                self.state_label.setText("State: -")
                self.key_label.setText("Key: -")
                self.detail_text.setPlainText("")
                return

            item = self.options[row]
            enabled = values.get(item.key, item.default)
            self.state_label.setText(f"State: {'ENABLED' if enabled else 'DISABLED'}")
            self.key_label.setText(f"Key: {item.key}")
            self.detail_text.setPlainText(f"{item.title}\n\n{item.description}")

        def _on_selection_changed(self) -> None:
            rows = self._selected_rows()

            if len(rows) == 1:
                self._show_detail(rows[0])
                return

            if len(rows) > 1:
                self.state_label.setText(f"State: {len(rows)} items selected")
                self.key_label.setText("Key: <multiple>")
                self.detail_text.setPlainText("Multiple options selected.\nUse Toggle Selected to flip all selected entries.")
                return

            self._show_detail(-1)

        def _on_item_changed(self, changed_item) -> None:
            if self._updating or changed_item is None:
                return

            if changed_item.column() != 0:
                return

            row = changed_item.row()

            if row < 0 or row >= len(self.options):
                return

            self.options[row]
            values[self.options[row].key] = changed_item.checkState() == qt_checked
            self._on_selection_changed()
            update_summary()

        def refresh(self, keep_selection: bool = True) -> None:
            prev_rows = self._selected_rows() if keep_selection else []
            self._updating = True

            self.table.setRowCount(len(self.options))

            for row, item in enumerate(self.options):
                enabled = values.get(item.key, item.default)
                check_item = self.table.item(row, 0)

                if check_item is None:
                    check_item = QtWidgets.QTableWidgetItem("")
                    check_item.setFlags(qt_item_enabled | qt_item_selectable | qt_item_checkable)
                    self.table.setItem(row, 0, check_item)

                check_item.setCheckState(qt_checked if enabled else qt_unchecked)

                title_item = self.table.item(row, 1)
                if title_item is None:
                    title_item = QtWidgets.QTableWidgetItem(item.title)
                    title_item.setFlags(qt_item_enabled | qt_item_selectable)
                    self.table.setItem(row, 1, title_item)
                else:
                    title_item.setText(item.title)

            self._updating = False

            self.table.clearSelection()
            if keep_selection:
                for row in prev_rows:
                    if 0 <= row < len(self.options):
                        self.table.selectRow(row)

            self._on_selection_changed()
            update_summary()

        def toggle_selected(self) -> None:
            rows = self._selected_rows()
            if not rows:
                return

            self._updating = True
            for row in rows:
                item = self.options[row]
                new_state = not values.get(item.key, item.default)
                values[item.key] = new_state
                check_item = self.table.item(row, 0)
                if check_item is not None:
                    check_item.setCheckState(qt_checked if new_state else qt_unchecked)
            self._updating = False
            self._on_selection_changed()
            update_summary()

        def enable_all(self) -> None:
            self._updating = True
            for row, item in enumerate(self.options):
                values[item.key] = True
                check_item = self.table.item(row, 0)
                if check_item is not None:
                    check_item.setCheckState(qt_checked)
            self._updating = False
            self._on_selection_changed()
            update_summary()

        def disable_all(self) -> None:
            self._updating = True
            for row, item in enumerate(self.options):
                values[item.key] = False
                check_item = self.table.item(row, 0)
                if check_item is not None:
                    check_item.setCheckState(qt_unchecked)
            self._updating = False
            self._on_selection_changed()
            update_summary()

    clks_panel = _SectionPanel("CLKS Features", clks_options)
    user_panel = _SectionPanel("User Apps", user_options)
    tabs.addTab(clks_panel, "CLKS")
    tabs.addTab(user_panel, "USER")
    update_summary()

    footer = QtWidgets.QHBoxLayout()
    footer.addWidget(QtWidgets.QLabel("Tip: select rows and click Toggle Selected."))
    footer.addStretch(1)

    save_btn = QtWidgets.QPushButton("Save and Exit")
    quit_btn = QtWidgets.QPushButton("Quit without Saving")
    footer.addWidget(save_btn)
    footer.addWidget(quit_btn)
    root_layout.addLayout(footer)

    def _on_save() -> None:
        result["save"] = True
        dialog.accept()

    def _on_quit() -> None:
        result["save"] = False
        dialog.reject()

    save_btn.clicked.connect(_on_save)
    quit_btn.clicked.connect(_on_quit)

    dialog.exec()

    if owns_app:
        app.quit()

    return result["save"]


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
    parser.add_argument("--gui", action="store_true", help="use GUI window mode (PySide)")
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

    if args.gui and args.plain:
        raise RuntimeError("--gui and --plain cannot be used together")

    clks_options = load_clks_options()
    user_options = discover_user_apps()
    all_options = clks_options + user_options

    previous = load_previous_values()
    values = init_values(all_options, previous, use_defaults=args.defaults)
    parse_set_overrides(values, args.set)

    should_save = args.non_interactive
    if not args.non_interactive:
        if args.gui:
            should_save = interactive_menu_gui(clks_options, user_options, values)
        else:
            if not sys.stdin.isatty():
                raise RuntimeError("menuconfig requires interactive tty (or use --non-interactive or --gui)")
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
