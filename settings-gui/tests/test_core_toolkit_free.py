#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""core/ is shared by the Qt and GTK windows, so nothing it imports may pull in either toolkit."""

import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

GUI_DIR = pathlib.Path(__file__).resolve().parent.parent

# Runs in a fresh interpreter: Qt or GTK already imported by another test would hide the import.
BLOCK_TOOLKITS = """
import importlib, importlib.abc, sys
BLOCKED = ("qtpy", "PyQt5", "PyQt6", "PySide2", "PySide6", "gi")
class Block(importlib.abc.MetaPathFinder):
    def find_spec(self, name, path, target=None):
        if name.split(".")[0] in BLOCKED:
            raise ImportError("toolkit import: " + name)
sys.meta_path.insert(0, Block())
for module in sys.argv[1:]:
    importlib.import_module(module)
"""


def import_without_toolkits(modules, path):
    env = dict(os.environ, PYTHONPATH=os.pathsep.join(str(p) for p in path))
    return subprocess.run(
        [sys.executable, "-c", BLOCK_TOOLKITS, *modules], env=env, capture_output=True, text=True
    )


class CoreToolkitFreeTest(unittest.TestCase):
    def test_core_imports_no_toolkit(self):
        modules = sorted(f"core.{p.stem}" for p in (GUI_DIR / "core").glob("*.py"))
        self.assertIn("core.catalog", modules)
        result = import_without_toolkits(modules, [GUI_DIR])
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_check_catches_an_indirect_toolkit_import(self):
        with tempfile.TemporaryDirectory() as tmp:
            pathlib.Path(tmp, "uses_widget.py").write_text("import ui.components\n")
            result = import_without_toolkits(["uses_widget"], [tmp, GUI_DIR])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("toolkit import: qtpy", result.stderr)


if __name__ == "__main__":
    unittest.main()
