#!/usr/bin/env python3
"""Tests for reading the typing log and the app test in the check dialog."""

import os
import tempfile
import unittest
from unittest import mock

# isort: off
from support import FakeDBusHandler, app  # noqa: F401
from core import typing_log
from core.typing_log import RIGHT, UNKNOWN, WRONG

# isort: on

HEADER = "# Ngó Sen 1.4.2 typing log.\n# desktop=KDE session=wayland\n"
KATE = "0.0\tfield\tfrontend=wayland program=kate surrounding=1 preedit=1 mode=Sen\n"
EDGE = "9.0\tfield\tfrontend=xim program=edge surrounding=0 preedit=0 mode=Sen\n"


def log(*lines):
    return HEADER + "".join(lines)


class ParseTest(unittest.TestCase):
    def test_sample_in_the_app_text_is_right(self):
        test = typing_log.parse(
            "x",
            log(
                KATE,
                "1.0\tkey\tdown 0x0074 t\n",
                "2.0\tdelete\toffset=-1 size=1\n",
                '3.0\tcommit\t"ế"\n',
                '4.0\tapp\t"Tiếng Việt có dấu|" cursor=17 anchor=17 length=17\n',
            ),
        )
        self.assertEqual(
            (test.program, test.frontend, test.surrounding, test.preedit),
            ("kate", "wayland", True, True),
        )
        self.assertEqual(test.deletions, {"delete": 1})
        self.assertEqual(test.verdict, RIGHT)
        self.assertIn("result: right", test.summary())

    def test_other_text_in_the_app_is_wrong(self):
        test = typing_log.parse(
            "x",
            log(
                KATE,
                "1.0\tforward\tBackSpace down\n",
                "1.1\tforward\tBackSpace up\n",
                "2.0\tpress\tcount=2 failed\n",
                '4.0\tapp\t"Tieếng Viêt có dâu|" cursor=18 anchor=18 length=18\n',
            ),
        )
        self.assertEqual(test.verdict, WRONG)
        self.assertEqual(test.deletions, {"forward": 1, "press": 1})
        self.assertEqual(test.failed_presses, 1)

    def test_field_cleared_after_sending_is_still_right(self):
        test = typing_log.parse(
            "x",
            log(
                KATE,
                '4.0\tapp\t"Tiếng Việt có dấu|" cursor=17 anchor=17 length=17\n',
                '5.0\tapp\t"|" cursor=0 anchor=0 length=0\n',
            ),
        )
        self.assertEqual(test.verdict, RIGHT)

    def test_only_an_empty_field_asks_the_user(self):
        test = typing_log.parse("x", log(KATE, '5.0\tapp\t"|" cursor=0 anchor=0 length=0\n'))
        self.assertEqual(test.verdict, UNKNOWN)

    def test_only_the_last_app_typed_into_counts(self):
        test = typing_log.parse(
            "x",
            log(
                KATE,
                '1.0\tapp\t"Tiếng Việt có dấu|" cursor=17 anchor=17 length=17\n',
                EDGE,
                '10.0\tcommit\t"có"\n',
            ),
        )
        self.assertEqual((test.program, test.surrounding), ("edge", False))
        self.assertEqual(test.app_text, "")
        self.assertEqual(test.verdict, UNKNOWN)

    def test_log_without_typing(self):
        test = typing_log.parse("x", HEADER)
        self.assertEqual((test.program, test.verdict), ("", UNKNOWN))


class LatestLogTest(unittest.TestCase):
    def test_picks_the_newest_typing_log(self):
        with tempfile.TemporaryDirectory() as state:
            directory = typing_log.log_dir({"XDG_STATE_HOME": state})
            os.makedirs(directory)
            for i, name in enumerate(
                ["typing-20261010-100000.log", "typing-20261011-100000.log", "notes.log"]
            ):
                path = os.path.join(directory, name)
                with open(path, "w") as f:
                    f.write(log(KATE))
                os.utime(path, (1000 + i, 1000 + i))
            self.assertEqual(
                os.path.basename(typing_log.latest_log(directory)), "typing-20261011-100000.log"
            )
            self.assertEqual(typing_log.latest_log(os.path.join(state, "missing")), "")


class CheckPageAppTestTest(unittest.TestCase):
    def test_user_answers_when_the_app_hides_its_text(self):
        from ui.pages.check import CheckPage

        with tempfile.TemporaryDirectory() as state:
            env = {
                "XDG_STATE_HOME": state,
                "XDG_SESSION_TYPE": "wayland",
                "XDG_CURRENT_DESKTOP": "KDE",
            }
            with mock.patch.dict(os.environ, env, clear=True):
                dialog = CheckPage(FakeDBusHandler(), "1.4.2", lambda *a: None, lambda n: None)
                dialog.read_typing_log()
                self.assertIsNone(dialog.app_test)
                self.assertTrue(dialog.right_btn.isHidden())

                os.makedirs(typing_log.log_dir())
                with open(
                    os.path.join(typing_log.log_dir(), "typing-20261015-100000.log"), "w"
                ) as f:
                    f.write(log(EDGE))
                dialog.read_typing_log()
            self.assertFalse(dialog.wrong_btn.isHidden())
            self.assertIn("edge (xim)", dialog.app_result.text())

            dialog.wrong_btn.click()
            self.assertTrue(dialog.wrong_btn.isHidden())
            self.assertIn("App test: edge (xim)", dialog.report_text())
            self.assertIn("result: wrong", dialog.report_text())


if __name__ == "__main__":
    unittest.main()
