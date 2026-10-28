# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import ast
import importlib.util
import pathlib
import unittest

import qt_api


def installed(*modules):
    return lambda name: name in modules


class QtApiTest(unittest.TestCase):
    def test_qt6_wins_over_an_installed_pyqt5(self):
        env = {}
        qt_api.pick(env, installed("PyQt5", "PyQt6"))
        self.assertEqual(env, {"QT_API": "pyqt6"})

    def test_pyside6_when_there_is_no_pyqt6(self):
        env = {}
        qt_api.pick(env, installed("PyQt5", "PySide6"))
        self.assertEqual(env, {"QT_API": "pyside6"})

    def test_a_chosen_binding_is_kept(self):
        env = {"QT_API": "pyqt5"}
        qt_api.pick(env, installed("PyQt5", "PyQt6"))
        self.assertEqual(env, {"QT_API": "pyqt5"})

    def test_qt5_only_is_left_to_qtpy(self):
        env = {}
        qt_api.pick(env, installed("PyQt5"))
        self.assertEqual(env, {})


class FallbackTest(unittest.TestCase):
    def test_an_installed_qtpy_is_used(self):
        modules = {}
        self.assertFalse(qt_api.install_fallback(modules, installed("qtpy", "PyQt6")))
        self.assertEqual(modules, {})

    def test_only_pyqt6_is_covered(self):
        modules = {}
        self.assertFalse(qt_api.install_fallback(modules, installed("PySide6", "PyQt5")))
        self.assertEqual(modules, {})

    @unittest.skipUnless(importlib.util.find_spec("PyQt6"), "PyQt6 is not installed")
    def test_every_name_the_window_imports_is_covered(self):
        modules = {}
        self.assertTrue(qt_api.install_fallback(modules, installed("PyQt6")))
        root = pathlib.Path(__file__).resolve().parent.parent
        missing = []
        for path in root.rglob("*.py"):
            for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
                if isinstance(node, ast.ImportFrom) and (node.module or "").startswith("qtpy."):
                    module = modules.get(node.module)
                    missing += [
                        f"{node.module}.{a.name}" for a in node.names if not hasattr(module, a.name)
                    ]
        self.assertEqual(missing, [])
        self.assertTrue(hasattr(modules["qtpy.QtCore"].Qt, "AlignCenter"))


if __name__ == "__main__":
    unittest.main()
