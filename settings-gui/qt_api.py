# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Picks the Qt binding before qtpy is imported."""

import enum
import importlib
import importlib.util
import os
import sys
import types

QT6_BINDINGS = (("pyqt6", "PyQt6"), ("pyside6", "PySide6"))
FALLBACK_MODULES = ("QtCore", "QtGui", "QtWidgets", "QtTest")


def pick(env=os.environ, installed=importlib.util.find_spec):
    """Prefers Qt 6, which the packages depend on."""
    # qtpy tries PyQt5 first, and a PyQt5 another package pulled in may lack the Wayland plugin.
    if env.get("QT_API"):
        return
    for api, module in QT6_BINDINGS:
        if installed(module):
            env["QT_API"] = api
            return


def promote_enums(module, wrapper_type):
    """Lets PyQt6 enum members be reached from their class, as qtpy does (Qt.AlignCenter)."""
    for class_name in dir(module):
        klass = getattr(module, class_name)
        if not class_name.startswith("Q") or not isinstance(klass, wrapper_type):
            continue
        for attrib_name in dir(klass):
            attrib = getattr(klass, attrib_name, None) if attrib_name[0].isupper() else None
            if isinstance(attrib, enum.EnumMeta):
                for name, value in attrib.__members__.items():
                    setattr(klass, name, value)


def install_fallback(modules=sys.modules, installed=importlib.util.find_spec):
    """Stands in for qtpy on PyQt6 where qtpy is not packaged, such as deepin."""
    if installed("qtpy") or not installed("PyQt6"):
        return False
    from PyQt6 import sip

    package = types.ModuleType("qtpy")
    package.__path__ = []
    package.API_NAME = "PyQt6"
    modules["qtpy"] = package
    for name in FALLBACK_MODULES:
        try:
            module = importlib.import_module(f"PyQt6.{name}")
        except ImportError:
            continue
        promote_enums(module, sip.wrappertype)
        setattr(package, name, module)
        modules[f"qtpy.{name}"] = module
    package.QtCore.Signal = package.QtCore.pyqtSignal
    package.QtCore.Slot = package.QtCore.pyqtSlot
    return True


pick()
install_fallback()
