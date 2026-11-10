# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reads the typing log the engine saves from its menu, to tell how one app took the sample text."""

import os
import re
from dataclasses import dataclass, field

from i18n import _

SAMPLE = "Tiếng Việt có dấu"
SAMPLE_TELEX = "Tieengs Vieetj cos daaus"

RIGHT, WRONG, UNKNOWN = "right", "wrong", "unknown"


@dataclass
class AppTest:
    path: str
    program: str = ""
    frontend: str = ""
    surrounding: bool = False
    preedit: bool = False
    mode: str = ""
    deletions: dict = field(default_factory=dict)
    failed_presses: int = 0
    app_text: str = ""
    verdict: str = UNKNOWN

    def summary(self):
        flags = f"surrounding={int(self.surrounding)} preedit={int(self.preedit)} mode={self.mode or '?'}"
        erase = ", ".join(f"{k} {n}" for k, n in sorted(self.deletions.items())) or "none"
        if self.failed_presses:
            erase += f" ({self.failed_presses} key presses failed)"
        line = f"{self.program or '?'} ({self.frontend or '?'}) {flags}; erased with: {erase}"
        if self.app_text:
            line += f'; app text: "{self.app_text}"'
        return f"{line}; result: {self.verdict}"


def log_dir(env=None):
    env = os.environ if env is None else env
    state = env.get("XDG_STATE_HOME") or os.path.join(env.get("HOME", ""), ".local", "state")
    return os.path.join(state, "ngosen")


def latest_log(directory):
    try:
        names = [n for n in os.listdir(directory) if re.fullmatch(r"typing-[\d-]+\.log", n)]
    except OSError:
        return ""
    paths = [os.path.join(directory, n) for n in names]
    return max(paths, key=os.path.getmtime, default="")


def parse(path, text):
    """The last app typed into before the log was saved: the events after its last field line."""
    events = []
    for line in text.splitlines():
        parts = line.split("\t", 2)
        if len(parts) == 3 and not line.startswith("#"):
            events.append((parts[1], parts[2]))
    starts = [i for i, (kind, _) in enumerate(events) if kind == "field"]
    test = AppTest(path)
    if not starts:
        return test
    fields = dict(re.findall(r"(\w+)=(\S*)", events[starts[-1]][1]))
    test.program = fields.get("program", "")
    test.frontend = fields.get("frontend", "")
    test.surrounding = fields.get("surrounding") == "1"
    test.preedit = fields.get("preedit") == "1"
    test.mode = fields.get("mode", "")
    for kind, detail in events[starts[-1] + 1 :]:
        if kind == "delete" or kind == "press" or (kind == "forward" and detail.endswith("down")):
            test.deletions[kind] = test.deletions.get(kind, 0) + 1
            test.failed_presses += kind == "press" and detail.endswith("failed")
        elif kind == "app":
            match = re.match(r'"(.*)" cursor=', detail)
            # An empty field says nothing: chat apps clear it once the message is sent.
            if match and match.group(1).replace("|", "").strip():
                test.app_text = match.group(1)
                if SAMPLE in test.app_text.replace("|", ""):
                    test.verdict = RIGHT
    if test.app_text and test.verdict != RIGHT:
        test.verdict = WRONG
    return test


def read_latest(env=None):
    path = latest_log(log_dir(env))
    if not path:
        return None
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return parse(path, f.read())
    except OSError:
        return None


def describe(test):
    """What the check dialog says about a read log, or about finding none."""
    if test is None:
        return _("No typing log found. Save one from the Ngó Sen menu first.")
    if not test.program and not test.frontend:
        return _("The log has no typing in it. Type the sample, then save the log again.")
    app = f"{test.program or '?'} ({test.frontend})"
    text = {
        RIGHT: _("{app} got the sample right."),
        WRONG: _("{app} got the sample wrong."),
        UNKNOWN: _("{app} does not tell Ngó Sen its text. Did the sample come out right?"),
    }[test.verdict].format(app=app)
    if test.verdict == WRONG and test.app_text:
        text += " " + _("The app shows: {}").format(test.app_text.replace("|", ""))
    return text
