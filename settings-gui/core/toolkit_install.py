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

# Kept in step with settings_window_packages in install.sh. A tuple lists alternatives, first wins.
PACKAGES = {
    "gtk": {
        "apt-get": ("gir1.2-adw-1", "python3-gi"),
        "dnf": ("python3-gobject", "gobject-introspection", "libadwaita"),
        "zypper": ("python3-gobject", "typelib-1_0-Adw-1"),
        "pacman": ("python-gobject", "libadwaita"),
    },
    "qt": {
        # Without the SVG plugins the logo and icons are blank. Debian 13 moved them out of
        # libqt6svg6, and deepin packages no qtpy.
        "apt-get": ("python3-qtpy", "python3-pyqt6", ("qt6-svg-plugins", "libqt6svg6")),
        "dnf": ("python3-QtPy", "python3-pyqt6", "qt6-qtsvg"),
        "zypper": ("python3-QtPy", "python3-PyQt6", "libQt6Svg6"),
        "pacman": ("python-qtpy", "python-pyqt6", "qt6-svg"),
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
    if find("qtpy"):
        return any(find(module) for module in QT_BINDINGS)
    # qt_api stands in for qtpy on PyQt6.
    return bool(find("PyQt6"))


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


def install_command(manager, packages):
    return [*INSTALL[manager], *packages]


def shown_command(manager, packages):
    return "sudo " + shlex.join(install_command(manager, packages))


def _output(args, run, ok=(0,)):
    """What the program printed, or None when it failed."""
    # The labels parsed below are translated in other locales.
    env = {**os.environ, "LC_ALL": "C"}
    try:
        result = run(args, capture_output=True, text=True, timeout=60, env=env)
    except (OSError, subprocess.SubprocessError):
        return None
    return result.stdout if result.returncode in ok else None


def download_size(manager, packages, run=subprocess.run):
    """Bytes to download, or None when the package manager cannot tell without root."""
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


def _apt_policy(names, run):
    """Whether each package is installed and whether it has a candidate; None when apt fails."""
    text = _output(["apt-cache", "policy", *names], run)
    if text is None:
        return None
    found = re.findall(r"^(\S+):\n +Installed: (\S+)\n +Candidate: (\S+)", text, re.MULTILINE)
    return {
        name: (installed != "(none)", candidate != "(none)") for name, installed, candidate in found
    }


def needed_packages(toolkit, manager, run=subprocess.run):
    """The packages still to install, each alternative narrowed to one the system has."""
    wanted = [
        entry if isinstance(entry, tuple) else (entry,) for entry in PACKAGES[toolkit][manager]
    ]
    state = (
        _apt_policy([name for entry in wanted for name in entry], run)
        if manager == "apt-get"
        else None
    )
    # Empty when the package lists were never fetched; the command works after apt-get update.
    if not state:
        return [entry[0] for entry in wanted]
    needed = []
    for entry in wanted:
        # The first one apt has decides: libqt6svg6 can be installed without the plugins.
        chosen = next((name for name in entry if state.get(name, (False, False))[1]), None)
        if chosen and not state[chosen][0]:
            needed.append(chosen)
    return needed


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
        self.packages = needed_packages(self.target, self.manager, run) if self.installable else []
        self.installable = bool(self.packages)
        self.runnable = self.installable and bool(which("pkexec"))

    @property
    def command(self):
        return shown_command(self.manager, self.packages)

    @property
    def install_argv(self):
        return ["pkexec", *install_command(self.manager, self.packages)]

    def download_size(self, run=subprocess.run):
        return download_size(self.manager, self.packages, run)

    def installed_now(self):
        return INSTALLED[self.target]()


def format_size(size):
    megabytes = size / 1024**2
    if megabytes < 0.1:
        return f"{size / 1024:.0f} kB"
    text = f"{megabytes:.1f}" if megabytes < 10 else f"{megabytes:.0f}"
    return text + " MB"


def switch(model, toolkit):
    """Saves the choice; the window reopens in it once its main loop ends."""
    model.set("SettingsToolkit", NAMES[toolkit])
    if not model.save():
        return False
    _reopen_in[:] = [toolkit]
    return True


def reopen_if_switched(execve=os.execve, argv=None):
    if not _reopen_in:
        return
    # Named outright too, so the new window does not depend on fcitx5 having written the file yet.
    env = {**os.environ, "NGOSEN_SETTINGS_TOOLKIT": _reopen_in[0]}
    execve(sys.executable, [*_without_page(argv or sys.orig_argv), "--page", "appearance"], env)


def _without_page(argv):
    kept, skip = [], False
    for arg in argv:
        if skip:
            skip = False
        elif arg == "--page":
            skip = True
        elif not arg.startswith("--page="):
            kept.append(arg)
    return kept
