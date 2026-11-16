# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Remembers the login that first saw each step waiting for a new login.

A step still waiting in a later login was overridden after login, so the check reports it as
failed instead of asking to log in again.
"""

import json
import os
import subprocess

from core import check_reminder, system_check
from core.typing_log import log_dir

VERSION = 1
BOOT_ID = "/proc/sys/kernel/random/boot_id"


def state_file(env=None):
    return os.path.join(log_dir(env), "login-waits.json")


def login_id(env, boot_id_path=BOOT_ID):
    """Empty when unknown, so a step is never reported as left over by mistake."""
    # Session numbers start over after a restart, so the boot tells them apart.
    session = env.get("XDG_SESSION_ID") or _display_session()
    try:
        with open(boot_id_path, encoding="ascii") as f:
            boot = f.read().strip()
    except OSError:
        return ""
    return f"{boot}/{session}" if boot and session else ""


def _display_session():
    """The session logind shows the user's desktop in, for apps started without XDG_SESSION_ID."""
    # GNOME and Plasma start apps from the systemd user manager, which outlives one login.
    cmd = ["loginctl", "show-user", str(os.getuid()), "--property=Display", "--value"]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
    except (OSError, subprocess.SubprocessError):
        return ""
    return out.stdout.strip()


def run(dbus_handler, ngosen_version="", env=None, path=None, boot_id_path=None):
    """Checks the machine for the check page: its facts and its steps."""
    env = dict(os.environ if env is None else env)
    facts = system_check.collect(dbus_handler, ngosen_version, env)
    facts.login = login_id(env, boot_id_path or BOOT_ID)
    return facts, steps(facts, path or state_file(env))


def steps(facts, path):
    waits, writable = _read(path)
    facts.login_waits = waits
    found = system_check.steps(facts)
    # With fcitx5 off the check sees nothing else, which says nothing about the waiting steps.
    if not facts.login or not writable or not facts.fcitx_running:
        return found
    now = {t: waits.get(t, facts.login) for f in found for t in f.waiting}
    if now != waits:
        check_reminder.write_json(path, {"version": VERSION, "waits": now})
    return found


def _read(path):
    """The remembered logins by step title, and whether the file may be written."""
    try:
        with open(path, encoding="utf-8") as f:
            state = json.load(f)
    except FileNotFoundError:
        return {}, True
    except (OSError, ValueError):
        return {}, True
    if not isinstance(state, dict) or not isinstance(state.get("version"), int):
        return {}, True
    if state["version"] > VERSION:
        return {}, False
    waits = state.get("waits")
    if state["version"] != VERSION or not isinstance(waits, dict):
        return {}, True
    return {k: v for k, v in waits.items() if isinstance(k, str) and isinstance(v, str)}, True
