# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""When the window opens on Check my machine by itself.

It does the first time it opens, and when a step that stops typing is not done: on the open that
finds the step, then every REMIND_EVERY opens while it stays undone.
"""

import json
import os

from core import system_check
from core.typing_log import log_dir

REMIND_EVERY = 3


def state_file(env=None):
    return os.path.join(log_dir(env), "check-reminders.json")


def blocking(found):
    """Steps that stop typing somewhere; one waiting only for a new login is already done."""
    return [f.title for f in found if f.level == system_check.FAIL and not f.after_login]


def should_open(found, path):
    """Counts this open and tells whether it lands on the check page."""
    state = _read(path)
    show = state is None
    seen = (state or {}).get("waiting", {})
    waiting = {}
    for title in blocking(found):
        opens = seen.get(title)
        if not isinstance(opens, int) or opens + 1 >= REMIND_EVERY:
            show = True
            waiting[title] = 0
        else:
            waiting[title] = opens + 1
    _write(path, {"waiting": waiting})
    return show


def _read(path):
    """None when the window never opened; a broken file counts as opened, so it does not nag."""
    try:
        with open(path, encoding="utf-8") as f:
            state = json.load(f)
    except FileNotFoundError:
        return None
    except (OSError, ValueError):
        return {}
    return state if isinstance(state, dict) else {}


def _write(path, state):
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(state, f, ensure_ascii=False)
    except OSError:
        pass
