# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the other settings window needs installed, how much it downloads, and the command for it."""

import importlib.util
import os
import re
import shlex
import shutil
import subprocess
import sys

QT_BINDINGS = ("PyQt6", "PySide6", "PyQt5")
MIN_ADW_MINOR = 5

# In a child process: the Qt window may open here instead, and its GTK 3 theme plugin aborts when
# GTK 4 is already loaded.
GTK_PROBE = f"""
import gi
gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw
raise SystemExit(0 if Adw.get_minor_version() >= {MIN_ADW_MINOR} else 1)
"""

# Kept in step with settings_window_packages in install.sh.
PACKAGES = {
    "gtk": {
        "apt-get": ("gir1.2-adw-1", "python3-gi"),
        "dnf": ("python3-gobject", "gobject-introspection", "libadwaita"),
        "zypper": ("python3-gobject", "typelib-1_0-Adw-1"),
        "pacman": ("python-gobject", "libadwaita"),
    },
    "qt": {
        "apt-get": ("python3-qtpy", "python3-pyqt6"),
        "dnf": ("python3-QtPy", "python3-pyqt6"),
        "zypper": ("python3-QtPy", "python3-PyQt6"),
        "pacman": ("python-qtpy", "python-pyqt6"),
    },
}

INSTALL = {
    "apt-get": ("apt-get", "install", "-y"),
    "dnf": ("dnf", "install", "-y"),
    "zypper": ("zypper", "--non-interactive", "install"),
    "pacman": ("pacman", "-S", "--needed", "--noconfirm"),
}

UNITS = {"B": 1, "KiB": 1024, "MiB": 1024**2, "GiB": 1024**3}


def has_qt(find=importlib.util.find_spec):
    # Packages installed while the window is open would stay unseen otherwise.
    importlib.invalidate_caches()
    return bool(find("qtpy")) and any(find(module) for module in QT_BINDINGS)


def has_gtk():
    """True when GTK 4 and libadwaita 1.5 or newer can be loaded."""
    try:
        result = subprocess.run([sys.executable, "-c", GTK_PROBE], capture_output=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return False
    return result.returncode == 0


INSTALLED = {"qt": has_qt, "gtk": has_gtk}


def package_manager(which=shutil.which):
    return next((name for name in INSTALL if which(name)), None)


def install_command(toolkit, manager):
    return [*INSTALL[manager], *PACKAGES[toolkit][manager]]


def shown_command(toolkit, manager):
    return "sudo " + shlex.join(install_command(toolkit, manager))


def _output(args, run, ok=(0,)):
    """What the program printed, or None when it failed."""
    # The labels parsed below are translated in other locales.
    env = {**os.environ, "LC_ALL": "C"}
    try:
        result = run(args, capture_output=True, text=True, timeout=60, env=env)
    except (OSError, subprocess.SubprocessError):
        return None
    return result.stdout if result.returncode in ok else None


def download_size(toolkit, manager, run=subprocess.run):
    """Bytes to download, or None when the package manager cannot tell without root."""
    packages = PACKAGES[toolkit][manager]
    if manager == "apt-get":
        return _apt_size(packages, run)
    if manager == "dnf":
        # The system cache, read only: a user's own dnf cache starts as a 100 MB copy of it.
        args = ["dnf", "-C", "--setopt=cachedir=/var/cache/libdnf5", "install", "--assumeno"]
        # Answering no makes dnf exit with 1.
        text = _output([*args, *packages], run, ok=(0, 1)) or ""
        if "Nothing to do" in text:
            return 0
        found = re.search(r"Need to download ([\d.]+) (B|KiB|MiB|GiB)", text)
        return round(float(found[1]) * UNITS[found[2]]) if found else None
    if manager == "pacman":
        text = _output(["pacman", "-Sp", "--needed", "--print-format", "%s", *packages], run)
        return None if text is None else sum(int(size) for size in text.split() if size.isdigit())
    return None


def _apt_size(packages, run):
    text = _output(["apt-get", "-s", "install", *packages], run)
    if text is None:
        return None
    names = re.findall(r"^Inst (\S+)", text, re.MULTILINE)
    if not names:
        return 0
    text = _output(["apt-cache", "show", "--no-all-versions", *names], run)
    if text is None:
        return None
    return sum(int(size) for size in re.findall(r"^Size: (\d+)$", text, re.MULTILINE))


def apt_has_libadwaita(run=subprocess.run):
    """Debian 12 has libadwaita 1.2, older than the GTK window needs."""
    text = _output(["apt-cache", "policy", "gir1.2-adw-1"], run) or ""
    found = re.search(r"^ *Candidate: (\S+)", text, re.MULTILINE)
    if not found or found[1] == "(none)":
        return False
    try:
        result = run(["dpkg", "--compare-versions", found[1], "ge", "1.5"], timeout=10)
    except (OSError, subprocess.SubprocessError):
        return False
    return result.returncode == 0


def can_install(toolkit, manager, run=subprocess.run):
    if manager is None:
        return False
    return toolkit != "gtk" or manager != "apt-get" or apt_has_libadwaita(run)


NAMES = {"qt": "Qt", "gtk": "GTK"}
_reopen_in = []


class Offer:
    """What switching from the running window to the other one takes on this machine."""

    def __init__(self, running, installed=INSTALLED, which=shutil.which, run=subprocess.run):
        self.target = "gtk" if running == "qt" else "qt"
        self.name = NAMES[self.target]
        self.ready = installed[self.target]()
        self.manager = package_manager(which)
        self.installable = not self.ready and can_install(self.target, self.manager, run)
        self.runnable = self.installable and bool(which("pkexec"))

    @property
    def packages(self):
        return PACKAGES[self.target][self.manager]

    @property
    def command(self):
        return shown_command(self.target, self.manager)

    @property
    def install_argv(self):
        return ["pkexec", *install_command(self.target, self.manager)]

    def download_size(self, run=subprocess.run):
        return download_size(self.target, self.manager, run)

    def installed_now(self):
        return INSTALLED[self.target]()


def format_size(size):
    megabytes = size / 1024**2
    text = f"{megabytes:.1f}" if megabytes < 10 else f"{megabytes:.0f}"
    return text + " MB"


def switch(model, toolkit):
    """Saves the choice; the window reopens in it once its main loop ends."""
    model.set("SettingsToolkit", NAMES[toolkit])
    if not model.save():
        return False
    _reopen_in[:] = [toolkit]
    return True


def reopen_if_switched(execve=os.execve):
    if not _reopen_in:
        return
    # Named outright too, so the new window does not depend on fcitx5 having written the file yet.
    env = {**os.environ, "NGOSEN_SETTINGS_TOOLKIT": _reopen_in[0]}
    execve(sys.executable, sys.orig_argv, env)
