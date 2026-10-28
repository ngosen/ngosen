#!/usr/bin/env python3
"""Tests for picking the settings window's language."""

import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import i18n  # noqa: E402


def write_catalog(localedir, strings):
    """A gettext .mo file holding the given English → Vietnamese strings."""
    strings = {"": "Content-Type: text/plain; charset=UTF-8\n", **strings}
    keys = sorted(strings)
    tables = b""
    data = b""
    offset = 28 + 16 * len(keys)
    for column in (keys, [strings[k] for k in keys]):
        for text in column:
            encoded = text.encode()
            tables += struct.pack("<II", len(encoded), offset + len(data))
            data += encoded + b"\0"
    header = struct.pack("<IIIIIII", 0x950412DE, 0, len(keys), 28, 28 + 8 * len(keys), 0, 0)
    path = os.path.join(localedir, "vi", "LC_MESSAGES")
    os.makedirs(path)
    with open(os.path.join(path, i18n.DOMAIN + ".mo"), "wb") as f:
        f.write(header + tables + data)


class LanguageTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.localedir = os.path.join(self.dir.name, "locale")
        write_catalog(self.localedir, {"Charset": "Bảng mã", "Settings": "Cài đặt"})

    def tearDown(self):
        i18n.use_language("English", localedir=self.dir.name)
        self.dir.cleanup()

    def write_config(self, text):
        conf = os.path.join(self.dir.name, "fcitx5", "conf")
        os.makedirs(conf, exist_ok=True)
        with open(os.path.join(conf, "ngosen.conf"), "w", encoding="utf-8") as f:
            f.write(text)
        return {"XDG_CONFIG_HOME": self.dir.name}

    def test_vietnamese_until_changed(self):
        self.assertEqual(i18n.configured_language({"XDG_CONFIG_HOME": self.dir.name}), "Vietnamese")
        env = self.write_config("# UiLanguage=Vietnamese\nIconTheme=Dark\n")
        self.assertEqual(i18n.configured_language(env), "Vietnamese")

    def test_reads_the_picked_language(self):
        env = self.write_config("IconTheme=Dark\nUiLanguage=English\n")
        self.assertEqual(i18n.configured_language(env), "English")

    def test_reads_a_config_that_is_not_utf8(self):
        env = self.write_config("UiLanguage=English\n")
        with open(os.path.join(self.dir.name, "fcitx5", "conf", "ngosen.conf"), "ab") as f:
            f.write(b"CustomKeymap=\xff\xfe\n")
        self.assertEqual(i18n.configured_language(env), "English")

    def test_vietnamese_on_any_desktop(self):
        i18n.use_language("Vietnamese", localedir=self.localedir)
        self.assertEqual(i18n._("Charset"), "Bảng mã")

    def test_english_even_when_fcitx5_runs_in_vietnamese(self):
        i18n.use_language("English", localedir=self.localedir)
        self.assertEqual(i18n._("Charset"), "Charset")
        self.assertEqual(i18n._("Bảng mã"), "Charset")


if __name__ == "__main__":
    unittest.main()
