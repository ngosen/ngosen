# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""What Đoán dấu learned on this machine: how much, showing it, and forgetting it."""

import os
from collections import namedtuple

from i18n import _

from core.ngosen_call import NO_REPLY, NOT_LOADED, NOT_RUNNING, TOO_OLD, CallError

# Where a run of typing started and ended, as timsen writes them.
START, END = "<s>", "</s>"

Status = namedtuple("Status", "line error table_missing")


def path(env=os.environ):
    data_home = env.get("XDG_DATA_HOME") or os.path.join(env.get("HOME", ""), ".local", "share")
    return os.path.join(data_home, "ngosen", "learned-diacritics")


def phrases(text):
    """(count, words) for each phrase in a saved file, most seen first."""
    found = []
    # The first line names the format.
    for line in text.splitlines()[1:]:
        fields = line.split("\t")
        if len(fields) < 3 or fields[0] not in ("1", "2", "3") or not fields[1].isdigit():
            continue
        words = fields[2:]
        # A start or end alone counts runs, not anything typed.
        if all(word in (START, END) for word in words):
            continue
        found.append((int(fields[1]), words))
    found.sort(key=lambda phrase: (-phrase[0], phrase[1]))
    return found


def shown(found):
    names = {START: _("(start)"), END: _("(end)")}
    return "\n".join(
        f"{count:>6}  {' '.join(names.get(word, word) for word in words)}" for count, words in found
    )


def remove_files(file):
    """Removes the file, its temporary copy and copies set aside as unreadable, as the engine does."""
    folder, name = os.path.split(file)
    folder = folder or "."
    for each in (file, os.path.join(folder, f".{name}.tmp")):
        try:
            os.remove(each)
        except FileNotFoundError:
            pass
    broken = f"{name}.broken"
    try:
        names = os.listdir(folder)
    except FileNotFoundError:
        return
    for each in names:
        rest = each[len(broken) :] if each.startswith(broken) else None
        if rest == "" or (rest and rest[0] == "." and rest[1:].isdigit()):
            try:
                os.remove(os.path.join(folder, each))
            except FileNotFoundError:
                pass
    # The removal lasts through a crash only once the folder is on disk.
    descriptor = os.open(folder, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


class Learned:
    """Asks the Ngó Sen that fcitx5 runs, which holds what it learned since its last save."""

    def __init__(self, dbus, file=None):
        self.dbus = dbus
        self.file = file or path()

    def _call(self, method):
        try:
            return self.dbus.call_ngosen(method), None
        except CallError as e:
            return None, e

    def _save(self):
        """A line saying why the running Ngó Sen could not save, or None."""
        result, _error = self._call("SaveLearned")
        # Without the call nothing newer than the file is held, or it cannot be had.
        if result is not None and str(result[0]) == "failed":
            return _("Could not save what was learned: {}").format(result[1])
        return None

    def _read(self):
        try:
            with open(self.file, encoding="utf-8", errors="replace") as f:
                return f.read()
        except FileNotFoundError:
            return ""

    def status(self):
        """A Status for the page: the line under the buttons, and whether the table is missing."""
        result, _error = self._call("DiacriticsStatus")
        missing = result is not None and str(result[0]) == "missing"
        if result is not None and str(result[1]):
            line = _("Could not read what was learned: {}. Forget all starts it over.")
            return Status(line.format(result[1]), True, missing)
        failed = self._save()
        if failed:
            return Status(failed, True, missing)
        try:
            count = len(phrases(self._read()))
        except OSError as e:
            return Status(
                _("Could not read what was learned: {}").format(e.strerror), True, missing
            )
        if not count:
            return Status(_("Nothing learned yet."), False, missing)
        return Status(_("Phrases learned: {}.").format(count), False, missing)

    def view(self):
        """(text to show, None), or (None, a line saying why it cannot be shown)."""
        failed = self._save()
        if failed:
            return None, failed
        try:
            found = phrases(self._read())
        except OSError as e:
            return None, _("Could not read what was learned: {}").format(e.strerror)
        return (shown(found) if found else _("Nothing learned yet.")), None

    def forget(self):
        """(whether it was forgotten, a line saying so or why not)."""
        result, error = self._call("ForgetLearned")
        if error is None:
            if result[0]:
                return True, _("Forgot everything Đoán dấu learned.")
            return False, _("Could not forget: {}").format(result[1])
        if error.name in NOT_RUNNING or error.name in NOT_LOADED:
            # Nothing runs that holds what was learned, so the file is all of it.
            try:
                remove_files(self.file)
            except OSError as e:
                return False, _("Could not forget: {}").format(e.strerror)
            return True, _("Forgot everything Đoán dấu learned.")
        if error.name == TOO_OLD:
            return False, _(
                "The running Ngó Sen is older than this window. Restart fcitx5, then try again."
            )
        if error.name == NO_REPLY:
            return False, _("Ngó Sen did not answer, so it may not have forgotten. Try again.")
        if error.name is None:
            return False, _("No desktop session bus, so Ngó Sen cannot be reached.")
        return False, _("Could not forget: {}").format(error.message or error.name)
