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

# Which chat abbreviations the user took back, as the engine names the file.
KEPT_SHORTHAND = "kept-chat-shorthand"

Status = namedtuple("Status", "line error table_missing")


def path(env=os.environ):
    data_home = env.get("XDG_DATA_HOME") or os.path.join(env.get("HOME", ""), ".local", "share")
    return os.path.join(data_home, "ngosen", "learned-diacritics")


def shown_path(file, home=None):
    home = home if home is not None else os.path.expanduser("~")
    return "~" + file[len(home) :] if home and file.startswith(home + os.sep) else file


def how_it_learns(file=None):
    """The lines that say what is learned, what is not, and where it is kept."""
    return [
        _(
            "It learns words with marks that Ngó Sen put into a field: words you typed with"
            " marks, and words you put back as typed with Backspace three times in a row after a"
            " wrong guess. Not words it guessed itself, and not text from macros."
        ),
        _(
            "A word without marks that Ngó Sen does not know, such as a name or khum, is learned"
            " once you typed it and went on typing three times, so fixing typos leaves it alone."
            " Only words of 2 to 10 letters, only while typos are fixed, and not in a terminal."
            " The count stays in memory and starts again when fcitx5 restarts."
        ),
        _(
            "It keeps runs of one, two and three words with how often each came, and where a"
            " run of typing started and ended. Put together, they can show short sentences you"
            " typed."
        ),
        _(
            "It learns when a run of typing ends: you leave the field, click, press Enter, or"
            " type 200 syllables. Text typed and then deleted with Backspace may still have"
            " been learned."
        ),
        _(
            "Words are kept in lower case with marks placed the new way, so View can look"
            " different from what you typed."
        ),
        _(
            "Never in password or private fields, nor in a terminal when Ngó Sen can tell it"
            " hides what you type, which it judges from signs and can miss. Nothing in Off"
            " mode, or with Đoán dấu or learning off; turning learning off stops it at once."
        ),
        _(
            "Kept in {} as plain text only you can read, saved every 5 minutes when there is"
            " something new and when fcitx5 quits. Nothing is sent anywhere."
        ).format(shown_path(file or path())),
        _(
            "A chat abbreviation you take back with Backspace twice in a row stays as typed."
            " Which ones is kept in {}, counted while learning is on, with Đoán dấu on or off;"
            " the Macros page lists them and can make one expand again."
        ).format(shown_path(os.path.join(os.path.dirname(file or path()), KEPT_SHORTHAND))),
        _("The typing log is separate: it is written only when you choose Save typing log."),
    ]


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
    """Removes the file, its temporary copy and copies set aside as unreadable, as the engine does.
    The chat abbreviations kept as typed, counted beside it, go too."""
    folder, name = os.path.split(file)
    folder = folder or "."
    kept = os.path.join(folder, KEPT_SHORTHAND)
    for each in (file, os.path.join(folder, f".{name}.tmp"), kept, kept + ".tmp"):
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
        """A line saying why the file may not hold all that was learned, or None."""
        result, error = self._call("SaveLearned")
        if error is not None:
            # Only with no Ngó Sen running is the file all there is.
            if error.name in NOT_RUNNING or error.name in NOT_LOADED:
                return None
            return _why(
                error,
                _("Ngó Sen did not answer, so this may not show all it learned. Try again."),
                _("Could not save what was learned: {}"),
            )
        if str(result[0]) == "failed":
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
        return False, _why(
            error,
            _("Ngó Sen did not answer, so it may not have forgotten. Try again."),
            _("Could not forget: {}"),
        )


def _why(error, no_reply, other):
    """Why a call to a Ngó Sen that may hold more than the file failed."""
    if error.name == TOO_OLD:
        return _("The running Ngó Sen is older than this window. Restart fcitx5, then try again.")
    if error.name == NO_REPLY:
        return no_reply
    if error.name is None:
        return _("No desktop session bus, so Ngó Sen cannot be reached.")
    return other.format(error.message or error.name)
