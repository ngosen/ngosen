# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""When the window opens on Check my machine by itself.

It does the first time it opens, and when a step that stops typing is not done: on the open that
finds the step, then 3, 6, 12... opens later while any such step stays undone.
"""

import json
import os
import tempfile

from core import system_check
from core.typing_log import log_dir

VERSION = 1
FIRST_GAP = 3


def state_file(env=None):
    return os.path.join(log_dir(env), "check-reminders.json")


def blocking(found):
    """Titles of the steps that stop typing; one waiting only for a new login is already done."""
    return [
        f.title
        for f in found
        if (f.level == system_check.FAIL and not f.after_login) or f.stops_typing
    ]


def should_open(found, path):
    """Counts this open and tells whether it lands on the check page."""
    state, writable = _read(path)
    if state is None:
        show, state = writable, _fresh()
    else:
        show = False
    now = blocking(found)
    if any(title not in state["known"] for title in now):
        show, state["opens"], state["gap"] = True, 0, FIRST_GAP
    elif now:
        state["opens"] += 1
        if state["opens"] >= state["gap"]:
            show, state["opens"], state["gap"] = True, 0, state["gap"] * 2
    else:
        state["opens"], state["gap"] = 0, FIRST_GAP
    # With fcitx5 off the check sees nothing else, which says nothing about the other steps.
    hidden = any(f.action == system_check.START_FCITX for f in found)
    state["known"] = sorted(set(now) | set(state["known"])) if hidden else now
    if writable and not _write(path, state):
        # Nothing remembers this open, so the next one must not look like the first.
        return bool(now)
    return show


def _fresh():
    return {"version": VERSION, "known": [], "opens": 0, "gap": FIRST_GAP}


def _read(path):
    """The state, None when the window never opened, and whether the file may be written."""
    try:
        with open(path, encoding="utf-8") as f:
            state = json.load(f)
    except FileNotFoundError:
        return None, True
    except (OSError, ValueError):
        return _fresh(), True
    if (
        isinstance(state, dict)
        and isinstance(state.get("version"), int)
        and state["version"] > VERSION
    ):
        return _fresh(), False
    if not _valid(state):
        return _fresh(), True
    return state, True


def _valid(state):
    return (
        isinstance(state, dict)
        and state.get("version") == VERSION
        and isinstance(state.get("known"), list)
        and all(isinstance(title, str) for title in state["known"])
        and isinstance(state.get("opens"), int)
        and isinstance(state.get("gap"), int)
        and state["gap"] >= FIRST_GAP
    )


def _write(path, state):
    """Replaces the file whole, so a write cut short leaves the last one."""
    folder = os.path.dirname(path)
    temp = None
    try:
        os.makedirs(folder, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", dir=folder, prefix=".check-reminders-", delete=False
        ) as f:
            temp = f.name
            json.dump(state, f, ensure_ascii=False)
        os.replace(temp, path)
    # ValueError: a title read from a hand-edited file that UTF-8 cannot hold.
    except (OSError, ValueError):
        if temp is not None:
            try:
                os.unlink(temp)
            except OSError:
                pass
        return False
    return True
