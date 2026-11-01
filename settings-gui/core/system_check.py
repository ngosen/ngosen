# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks how fcitx5 is set up on this machine and builds the text of a bug report.

The rules follow install.sh and https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland.
"""

import configparser
import os
import re
import subprocess
from dataclasses import dataclass, field
from urllib.parse import urlencode

from i18n import _

from core.brand import ISSUE_URL

IM_VARIABLES = ("GTK_IM_MODULE", "QT_IM_MODULE", "XMODIFIERS", "SDL_IM_MODULE")

OK, WARN, FAIL = "ok", "warn", "fail"

FORWARD_KEYS = "forward-keys@ngosen.github.io"
INPUT_PANEL = "kimpanel@kde.org"
INPUT_PANEL_PAGE = "https://extensions.gnome.org/extension/261/"


@dataclass
class Facts:
    env: dict
    fcitx_running: bool = False
    ngosen_in_group: bool = False
    # IBus took this session's address file, so apps and GNOME Shell talk to it, not fcitx5.
    ibus_owns_session: bool = False
    kwin_input_method: str = ""
    apps: list = field(default_factory=list)
    distro: str = ""
    fcitx_version: str = ""
    ngosen_version: str = ""
    addon_replaced: bool = False
    gnome_shell_version: str = ""
    # As gnome-extensions reports it (ACTIVE, INACTIVE, ERROR...), empty when not installed.
    forward_keys_state: str = ""
    input_panel_state: str = ""

    @property
    def session(self):
        return self.env.get("XDG_SESSION_TYPE", "").lower()

    @property
    def desktops(self):
        return [d.upper() for d in self.env.get("XDG_CURRENT_DESKTOP", "").split(":") if d]


@dataclass
class Finding:
    level: str
    title: str
    fix: str = ""


def parse_debug_info(text):
    """(program, frontend) of every input context fcitx5 lists, in order, without repeats."""
    apps = []
    for program, frontend in re.findall(r"program:(\S*) frontend:(\S+)", text):
        if program and (program, frontend) not in apps:
            apps.append((program, frontend))
    return apps


def _needs(facts, variable, level, title):
    if facts.env.get(variable) == "fcitx":
        return []
    return [
        Finding(level, title, _("Set {} in the session environment.").format(f"{variable}=fcitx"))
    ]


def evaluate(facts):
    if not facts.fcitx_running:
        return [
            Finding(
                FAIL,
                _("fcitx5 is not running"),
                _("Start Fcitx 5 from the application menu, or run fcitx5 -d."),
            )
        ]
    found = []
    if facts.addon_replaced:
        found.append(
            Finding(
                WARN,
                _("fcitx5 still runs Ngó Sen from before the update"),
                _("Log out and back in to start the installed version."),
            )
        )
    if not facts.ngosen_in_group:
        found.append(
            Finding(
                FAIL,
                _("Ngó Sen is not in the fcitx5 input method list"),
                _("Open Fcitx 5 Configuration and add Ngó Sen under Input Method."),
            )
        )
    if facts.env.get("XMODIFIERS") != "@im=fcitx":
        found.append(
            Finding(
                WARN,
                _("Older X11 apps have no input method"),
                _("Set {} in the session environment.").format("XMODIFIERS=@im=fcitx"),
            )
        )
    found += _desktop_findings(facts)
    if facts.ibus_owns_session:
        found.append(
            Finding(
                WARN,
                _("IBus is running next to fcitx5"),
                _("Two input methods compete for keys; stop IBus from starting at login."),
            )
        )
    return found


def _drops_forwarded_keys(version):
    """GNOME 50.0 to 50.3 drop the keys an input method sends on to the app (mutter#4853)."""
    match = re.match(r"50\.(\d+)$", version)
    return match is not None and int(match.group(1)) <= 3


def _forward_keys_findings(facts):
    if not _drops_forwarded_keys(facts.gnome_shell_version) or facts.forward_keys_state == "ACTIVE":
        return []
    if facts.forward_keys_state:
        fix = _(
            "Turn on the fix: gnome-extensions enable {}. If keys still drop, log out and back in."
        ).format(FORWARD_KEYS)
    else:
        fix = _(
            "Install Ngó Sen with install.sh, which adds the fix for this GNOME version, then log out and back in."
        )
    title = _("GNOME {} drops the keys Ngó Sen sends to apps").format(facts.gnome_shell_version)
    return [Finding(WARN, title, fix)]


def _input_panel_findings(facts):
    state = facts.input_panel_state
    if state == "ACTIVE":
        return []
    if not state:
        fix = _(
            "Run install.sh again, or install Input Method Panel from {}, then log out and back in."
        ).format(INPUT_PANEL_PAGE)
    elif state in ("ERROR", "OUT OF DATE"):
        fix = _("The installed Input Method Panel does not run; update it from {}.").format(
            INPUT_PANEL_PAGE
        )
    else:
        fix = _(
            "Turn on Input Method Panel: gnome-extensions enable {}. If it still does not show, log out and back in."
        ).format(INPUT_PANEL)
    # Only this extension draws fcitx5's popups inside GNOME Shell.
    return [Finding(WARN, _("Menu and hints do not show in GNOME's search box"), fix)]


def _desktop_findings(facts):
    if "GNOME" in facts.desktops:
        return (
            _needs(facts, "QT_IM_MODULE", FAIL, _("Qt apps have no input method"))
            + _needs(
                facts,
                "GTK_IM_MODULE",
                WARN,
                _("Chrome and older GTK apps may have no input method"),
            )
            + _forward_keys_findings(facts)
            + _input_panel_findings(facts)
        )
    if facts.session == "x11":
        return _needs(facts, "GTK_IM_MODULE", FAIL, _("GTK apps have no input method")) + _needs(
            facts, "QT_IM_MODULE", FAIL, _("Qt apps have no input method")
        )
    if facts.session != "wayland":
        return []
    if "KDE" in facts.desktops:
        found = []
        if "fcitx" not in facts.kwin_input_method.lower():
            found.append(
                Finding(
                    FAIL,
                    _("KDE does not start fcitx5 as its virtual keyboard"),
                    _("System Settings → Keyboard → Virtual Keyboard: choose Fcitx 5."),
                )
            )
        if facts.env.get("GTK_IM_MODULE") or facts.env.get("QT_IM_MODULE"):
            found.append(
                Finding(
                    WARN,
                    _("GTK_IM_MODULE or QT_IM_MODULE is set on KDE Wayland"),
                    _("KDE Wayland does not need them, and the candidate window may flicker."),
                )
            )
        return found
    return _needs(facts, "QT_IM_MODULE", WARN, _("Qt 5 apps have no input method"))


def report(facts, findings, app_test=None):
    """Plain text for the bug report; it lists app names, so the user reviews it before sending."""
    marks = {OK: "OK", WARN: "!", FAIL: "X"}
    desktop = ":".join(facts.desktops) or "?"
    lines = [
        f"Ngó Sen {facts.ngosen_version or '?'}, fcitx5 {facts.fcitx_version or '?'}",
        f"{facts.distro or '?'}, {desktop}, {facts.session or '?'}",
        " ".join(f"{v}={facts.env.get(v) or '(unset)'}" for v in IM_VARIABLES),
    ]
    if facts.kwin_input_method:
        lines.append(f"KWin InputMethod={facts.kwin_input_method}")
    if facts.gnome_shell_version:
        state = facts.forward_keys_state or "not installed"
        panel = facts.input_panel_state or "not installed"
        lines.append(
            f"GNOME Shell {facts.gnome_shell_version}, forward-keys: {state}, input panel: {panel}"
        )
    lines.append("")
    lines += [f"[{marks[f.level]}] {f.title}" for f in findings] or ["[OK] no setup problem found"]
    if facts.apps:
        lines += ["", "Apps: " + ", ".join(f"{p} ({fe})" for p, fe in facts.apps)]
    if app_test:
        lines += ["", "App test: " + app_test.summary()]
    return "\n".join(lines)


def issue_url(facts, text):
    query = {"template": "bug_report.yml", "diagnose": text}
    if facts.ngosen_version:
        query["version"] = facts.ngosen_version
    if facts.distro:
        query["distro"] = facts.distro
    return f"{ISSUE_URL}?{urlencode(query)}"


def collect(dbus_handler, ngosen_version="", env=None):
    env = dict(os.environ if env is None else env)
    facts = Facts(env={k: v for k, v in env.items() if k in IM_VARIABLES or k.startswith("XDG_")})
    facts.ngosen_version = ngosen_version
    facts.distro = _distro()
    facts.fcitx_version = _fcitx_version()
    facts.ibus_owns_session = ibus_owns_session(env)
    facts.kwin_input_method = _kwin_input_method(env)
    facts.addon_replaced = runs_replaced_addon()
    if "GNOME" in facts.desktops:
        facts.gnome_shell_version = _gnome_shell_version()
        facts.forward_keys_state = _extension_state(FORWARD_KEYS)
        facts.input_panel_state = _extension_state(INPUT_PANEL)
    names = dbus_handler.input_method_names()
    facts.fcitx_running = names is not None
    facts.ngosen_in_group = "ngosen" in (names or [])
    facts.apps = parse_debug_info(dbus_handler.debug_info())
    return facts


def _distro():
    try:
        with open("/etc/os-release", encoding="utf-8") as f:
            for line in f:
                if line.startswith("PRETTY_NAME="):
                    return line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    return ""


def _fcitx_version():
    try:
        out = subprocess.run(["fcitx5", "--version"], capture_output=True, text=True, timeout=3)
    except (OSError, subprocess.SubprocessError):
        return ""
    return out.stdout.strip()


def _output(cmd):
    # Untranslated labels, so "State:" can be found on any desktop language.
    env = {**os.environ, "LC_ALL": "C"}
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=3, env=env).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def _gnome_shell_version():
    return _output(["gnome-shell", "--version"]).strip().removeprefix("GNOME Shell ")


def _extension_state(uuid):
    match = re.search(r"^\s*State:\s*(.+?)\s*$", _output(["gnome-extensions", "info", uuid]), re.M)
    return match.group(1) if match else ""


def _pids_named(name, proc="/proc"):
    """Directories under proc of this user's processes called name."""
    try:
        pids = [p for p in os.listdir(proc) if p.isdigit()]
    except OSError:
        return []
    uid = os.getuid()
    found = []
    for pid in pids:
        path = os.path.join(proc, pid)
        try:
            if os.stat(path).st_uid != uid:
                continue
            # Any process can pick a name that is not UTF-8.
            with open(os.path.join(path, "comm"), encoding="utf-8", errors="replace") as f:
                if f.read().strip() == name:
                    found.append(path)
        except OSError:
            continue
    return found


def _machine_id():
    for path in ("/var/lib/dbus/machine-id", "/etc/machine-id"):
        try:
            with open(path, encoding="utf-8") as f:
                return f.read().strip()
        except OSError:
            continue
    return ""


def _ibus_address_name(env):
    """Display part of the address file name, as IBus builds it."""
    if env.get("WAYLAND_DISPLAY"):
        return "unix-" + env["WAYLAND_DISPLAY"]
    host, _, rest = env.get("DISPLAY", ":0").partition(":")
    return f"{host or 'unix'}-{rest.split('.')[0] or '0'}"


def ibus_owns_session(env, proc="/proc", machine_id=None):
    """Whether this session's IBus address file names a running ibus-daemon.

    fcitx5 writes its own pid there; another program's IBus, such as the one xpra starts for its
    own display, writes a file for that display and does not count.
    """
    path = env.get("IBUS_ADDRESS_FILE")
    if not path:
        config_home = env.get("XDG_CONFIG_HOME") or os.path.join(env.get("HOME", ""), ".config")
        name = f"{machine_id or _machine_id()}-{_ibus_address_name(env)}"
        path = os.path.join(config_home, "ibus", "bus", name)
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            match = re.search(r"^IBUS_DAEMON_PID=(\d+)", f.read(), re.M)
        if match is None:
            return False
        with open(
            os.path.join(proc, match.group(1), "comm"), encoding="utf-8", errors="replace"
        ) as f:
            return f.read().strip() == "ibus-daemon"
    except OSError:
        return False


def runs_replaced_addon(proc="/proc"):
    """Whether a running fcitx5 still uses a Ngó Sen library that an update has replaced.

    Updating the package does not restart fcitx5, so it keeps typing with the old code.
    """
    for path in _pids_named("fcitx5", proc):
        try:
            with open(os.path.join(path, "maps"), encoding="utf-8", errors="replace") as f:
                if any(line.rstrip().endswith("/libngosen.so (deleted)") for line in f):
                    return True
        except OSError:
            continue
    return False


def _kwin_input_method(env):
    config_home = env.get("XDG_CONFIG_HOME") or os.path.join(env.get("HOME", ""), ".config")
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        parser.read(os.path.join(config_home, "kwinrc"), encoding="utf-8")
    except configparser.Error:
        return ""
    return parser.get("Wayland", "InputMethod", fallback="")
