# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

# isort: off
from fake_fcitx import FakeDBusHandler
from core import dictionary

# isort: on


class DictionaryTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.bundled = os.path.join(tmp.name, "bundled.dict")
        with open(self.bundled, "w", encoding="utf-8") as f:
            f.write("# bundled\nhello\nworld\n")
        self.local = os.path.join(tmp.name, "fcitx5/ngosen/vietnamese.cm.dict")
        self.fake = FakeDBusHandler()
        self.dictionary = dictionary.Dictionary(self.fake, lambda: self.bundled)

    def write_local(self, data):
        os.makedirs(os.path.dirname(self.local), exist_ok=True)
        with open(self.local, "wb") as f:
            f.write(data)

    def test_reads_the_bundled_words_without_a_local_file(self):
        self.assertIsNone(self.dictionary.load())
        self.assertEqual(self.dictionary.words, ["hello", "world"])
        self.assertFalse(self.dictionary.is_modified_from_default())

    def test_save_writes_the_words_and_the_switch(self):
        self.dictionary.load()
        self.dictionary.words.append("ngosen")
        self.dictionary.enabled = True
        self.assertTrue(self.dictionary.save())
        with open(self.local, encoding="utf-8") as f:
            self.assertEqual(f.read(), "hello\nworld\nngosen\n")
        self.assertEqual(self.fake.values["EnableDictionary"], "True")
        self.assertFalse(self.dictionary.is_modified())

    def test_the_bundled_words_leave_no_local_copy(self):
        self.write_local(b"hello\nworld\nextra\n")
        self.dictionary.load()
        self.dictionary.words.remove("extra")
        self.assertTrue(self.dictionary.save())
        self.assertFalse(os.path.exists(self.local))

    def test_save_refused_after_a_failed_load(self):
        broken = b"hello\n\xff\xfe broken\n"
        self.write_local(broken)
        self.assertIsNotNone(self.dictionary.load())
        self.dictionary.words = ["hello"]
        with self.assertRaises(dictionary.LoadFailedError):
            self.dictionary.save()
        with open(self.local, "rb") as f:
            self.assertEqual(f.read(), broken)

    def test_a_failed_write_leaves_no_temp_file(self):
        real = dictionary.tempfile.NamedTemporaryFile

        class FullDisk:
            def __init__(self, *args, **kwargs):
                self.file = real(*args, **kwargs)
                self.name = self.file.name

            def __enter__(self):
                return self

            def __exit__(self, *exc):
                self.file.close()
                raise OSError(28, "No space left on device")

            def writelines(self, lines):
                self.file.writelines(lines)

            def write(self, text):
                self.file.write(text)

        self.dictionary.load()
        self.dictionary.words.append("ngosen")
        with mock.patch.object(dictionary.tempfile, "NamedTemporaryFile", FullDisk):
            with self.assertRaises(OSError):
                self.dictionary.save()
        self.assertEqual(os.listdir(os.path.dirname(self.local)), [])

    def test_reset_goes_back_to_the_bundled_words(self):
        self.write_local(b"mine\n")
        self.dictionary.load()
        self.dictionary.enabled = True
        self.dictionary.reset()
        self.assertEqual(self.dictionary.words, ["hello", "world"])
        self.assertFalse(self.dictionary.enabled)


if __name__ == "__main__":
    unittest.main()
