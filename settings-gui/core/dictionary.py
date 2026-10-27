# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The custom dictionary: words the engine keeps as typed, and whether it is on."""

import contextlib
import os
import tempfile

from core.settings_snapshot import local_dict_path

BUNDLED_PATHS = [
    "/usr/share/fcitx5/ngosen/vietnamese.cm.dict",
    "/usr/local/share/fcitx5/ngosen/vietnamese.cm.dict",
]


class LoadFailedError(Exception):
    """Saving would overwrite a file that was not read in full."""


def bundled_path():
    for path in BUNDLED_PATHS:
        if os.path.exists(path):
            return path
    return BUNDLED_PATHS[0]


def read_words(path):
    with open(path, encoding="utf-8") as f:
        return [w for w in (line.strip() for line in f) if w and not w.startswith("#")]


class Dictionary:
    def __init__(self, dbus, bundled=bundled_path):
        self.dbus = dbus
        self.bundled = bundled
        self.words = []
        self.enabled = False
        self.load_failed = False
        self.path = None
        self.saved_state = self.state()

    def load(self):
        """Reads the words and the switch. Returns the read error, after which saving is refused."""
        self.load_failed = False
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            self.enabled = str(values.get("EnableDictionary", "False")).lower() == "true"
        self.words = []
        error = None
        local = local_dict_path()
        path = self.path = local if os.path.exists(local) else self.bundled()
        if os.path.exists(path):
            try:
                self.words = read_words(path)
            except Exception as e:
                self.load_failed = True
                error = e
        self.saved_state = self.state()
        return error

    def bundled_words(self):
        try:
            return read_words(self.bundled())
        except OSError:
            return []

    def reset(self):
        self.enabled = False
        self.words = self.bundled_words()

    def is_modified_from_default(self):
        return self.enabled or sorted(self.words) != sorted(self.bundled_words())

    def state(self):
        return {"words": sorted(self.words), "EnableDictionary": self.enabled}

    def is_modified(self):
        return self.state() != self.saved_state

    def save(self):
        """Writes the switch and the words. False when fcitx5 cannot be reached.

        Raises LoadFailedError after a failed load, and OSError when the file cannot be written.
        """
        if self.load_failed:
            raise LoadFailedError()
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            values["EnableDictionary"] = "True" if self.enabled else "False"
            if not self.dbus.set_config(values):
                return False
        elif not self.dbus.iface:
            return False

        local = local_dict_path()
        if sorted(self.words) == sorted(self.bundled_words()):
            # A copy of the bundled words would hide later updates to them.
            if os.path.exists(local):
                os.remove(local)
        else:
            _write_atomically(local, self.words)
        self.dbus.reload_addon_config()
        self.saved_state = self.state()
        return True


def _write_atomically(path, words):
    folder = os.path.dirname(path)
    os.makedirs(folder, exist_ok=True)
    tf = tempfile.NamedTemporaryFile("w", dir=folder, encoding="utf-8", delete=False)
    try:
        with tf:
            tf.writelines(f"{word}\n" for word in words)
        os.replace(tf.name, path)
    except BaseException:
        with contextlib.suppress(OSError):
            os.remove(tf.name)
        raise
