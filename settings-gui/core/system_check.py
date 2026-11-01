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
# Not org.gnome.Shell.Extensions: that name starts the Extensions app, which exits without replying.
INPUT_PANEL_INSTALLER = "org.gnome.Shell"


@dataclass
class Facts:
    env: dict
    fcitx_running: bool = False
    ngosen_in_group: bool = False
    # False when fcitx5 started before Ngó Sen was installed, so it cannot add it yet.
    ngosen_loaded: bool = True
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
    # The running GNOME Shell has the extension's code that names apps, not just its files.
    app_names_running: bool = False
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
    # What the window can do about it (a fix action below), and the command to copy when that fails.
    action: str = ""
    command: str = ""


START_FCITX, RESTART_FCITX, ADD_NGOSEN, ENABLE_FORWARD_KEYS, INSTALL_INPUT_PANEL = (
    "start-fcitx",
    "restart-fcitx",
    "add-ngosen",
    "enable-forward-keys",
    "install-input-panel",
)


def parse_debug_info(text):
    """(program, frontend) of every input context fcitx5 lists, in order, without repeats."""
    apps = []
    for program, frontend in re.findall(r"program:(\S*) frontend:(\S+)", text):
        if program and (program, frontend) not in apps:
            apps.append((program, frontend))
    return apps


def _needs(facts, variable, level, title, ok_title):
    if facts.env.get(variable) == "fcitx":
        return [Finding(OK, ok_title)]
    return [
        Finding(level, title, _("Set {} in the session environment.").format(f"{variable}=fcitx"))
    ]


def steps(facts):
    """Every check that applies to this machine, passed or not, in the order to fix them."""
    if not facts.fcitx_running:
        return [
            Finding(
                FAIL,
                _("fcitx5 is not running"),
                _("Start Fcitx 5 from the application menu, or run fcitx5 -d."),
                START_FCITX,
                "fcitx5 -d",
            )
        ]
    found = [Finding(OK, _("fcitx5 is running"))]
    if facts.addon_replaced:
        found.append(_replaced_addon_finding(facts))
    if facts.ngosen_in_group:
        found.append(Finding(OK, _("Ngó Sen is in the fcitx5 input method list")))
    elif not facts.ngosen_loaded and not facts.addon_replaced:
        found.append(_restart_finding(facts, _("fcitx5 started before Ngó Sen was installed")))
    else:
        found.append(
            Finding(
                FAIL,
                _("Ngó Sen is not in the fcitx5 input method list"),
                _("Open Fcitx 5 Configuration and add Ngó Sen under Input Method."),
                ADD_NGOSEN,
                "fcitx5-configtool",
            )
        )
    if facts.env.get("XMODIFIERS") == "@im=fcitx":
        found.append(Finding(OK, _("Older X11 apps use fcitx5")))
    else:
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


def _replaced_addon_finding(facts):
    return _restart_finding(facts, _("fcitx5 still runs Ngó Sen from before the update"))


def _restart_finding(facts, title):
    # KWin starts fcitx5 as its virtual keyboard and ignores a restart asked over D-Bus.
    if facts.session == "wayland" and "KDE" in facts.desktops:
        return Finding(WARN, title, _("Log out and back in to start the installed version."))
    return Finding(
        WARN,
        title,
        _("Restart fcitx5, or log out and back in, to start the installed version."),
        RESTART_FCITX,
        "fcitx5 -rd",
    )


def evaluate(facts):
    """The steps that need fixing."""
    return [f for f in steps(facts) if f.level != OK]


def _drops_forwarded_keys(version):
    """GNOME 50.0 to 50.3 drop the keys an input method sends on to the app (mutter#4853)."""
    match = re.match(r"50\.(\d+)$", version)
    return match is not None and int(match.group(1)) <= 3


def _version_tuple(version):
    return tuple(int(part) for part in re.findall(r"\d+", version))


def _needs_shell_for_app_names(facts):
    """fcitx5 before 5.1.22 cannot name apps GNOME Shell relays; the extension loads on GNOME 46 to 50."""
    shell = _version_tuple(facts.gnome_shell_version)
    fcitx = _version_tuple(facts.fcitx_version)
    return bool(shell) and bool(fcitx) and 46 <= shell[0] <= 50 and fcitx < (5, 1, 22)


def _app_name_findings(facts):
    if facts.app_names_running:
        return [Finding(OK, _("Apps keep their own typing mode"))]
    title = _("Some apps cannot keep their own typing mode")
    # GNOME reads an extension's files only at login, so turning it on now cannot work yet.
    if facts.forward_keys_state in ("", "OUT OF DATE", "ACTIVE"):
        fix = _("Log out and back in. If this stays, install Ngó Sen with install.sh.")
        return [Finding(WARN, title, fix)]
    fix = _("Turn on the extension: gnome-extensions enable {}, then log out and back in.").format(
        FORWARD_KEYS
    )
    command = f"gnome-extensions enable {FORWARD_KEYS}"
    return [Finding(WARN, title, fix, ENABLE_FORWARD_KEYS, command)]


def _forward_keys_findings(facts):
    names = _app_name_findings(facts) if _needs_shell_for_app_names(facts) else []
    if not _drops_forwarded_keys(facts.gnome_shell_version):
        return names
    if facts.forward_keys_state == "ACTIVE":
        title = _("The fix for GNOME {} is on").format(facts.gnome_shell_version)
        return [Finding(OK, title)] + names
    title = _("GNOME {} drops the keys Ngó Sen sends to apps").format(facts.gnome_shell_version)
    if not facts.forward_keys_state:
        fix = _(
            "Install Ngó Sen with install.sh, which adds the fix for this GNOME version, then log out and back in."
        )
        return [Finding(WARN, title, fix)]
    fix = _(
        "Turn on the fix: gnome-extensions enable {}. If keys still drop, log out and back in."
    ).format(FORWARD_KEYS)
    command = f"gnome-extensions enable {FORWARD_KEYS}"
    return [Finding(WARN, title, fix, ENABLE_FORWARD_KEYS, command)]


def _input_panel_findings(facts):
    state = facts.input_panel_state
    if state == "ACTIVE":
        return [Finding(OK, _("Menu and hints show in GNOME's search box"))]
    title = _("Menu and hints do not show in GNOME's search box")
    # Only this extension draws fcitx5's popups inside GNOME Shell.
    if not state:
        fix = _(
            "Run install.sh again, or install Input Method Panel from {}, then log out and back in."
        ).format(INPUT_PANEL_PAGE)
        return [Finding(WARN, title, fix, INSTALL_INPUT_PANEL, f"xdg-open {INPUT_PANEL_PAGE}")]
    if state in ("ERROR", "OUT OF DATE"):
        fix = _("The installed Input Method Panel does not run; update it from {}.").format(
            INPUT_PANEL_PAGE
        )
        return [Finding(WARN, title, fix)]
    fix = _(
        "Turn on Input Method Panel: gnome-extensions enable {}. If it still does not show, log out and back in."
    ).format(INPUT_PANEL)
    return [Finding(WARN, title, fix)]


def _desktop_findings(facts):
    if "GNOME" in facts.desktops:
        return (
            _needs(
                facts,
                "QT_IM_MODULE",
                FAIL,
                _("Qt apps have no input method"),
                _("Qt apps use fcitx5"),
            )
            + _needs(
                facts,
                "GTK_IM_MODULE",
                WARN,
                _("Chrome and older GTK apps may have no input method"),
                _("Chrome and older GTK apps use fcitx5"),
            )
            + _forward_keys_findings(facts)
            + _input_panel_findings(facts)
        )
    if facts.session == "x11":
        return _needs(
            facts,
            "GTK_IM_MODULE",
            FAIL,
            _("GTK apps have no input method"),
            _("GTK apps use fcitx5"),
        ) + _needs(
            facts, "QT_IM_MODULE", FAIL, _("Qt apps have no input method"), _("Qt apps use fcitx5")
        )
    if facts.session != "wayland":
        return []
    if "KDE" in facts.desktops:
        found = []
        if "fcitx" in facts.kwin_input_method.lower():
            found.append(Finding(OK, _("KDE starts fcitx5 as its virtual keyboard")))
        else:
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
    return _needs(
        facts,
        "QT_IM_MODULE",
        WARN,
        _("Qt 5 apps have no input method"),
        _("Qt 5 apps use fcitx5"),
    )


def with_ngosen(layout, items):
    """The group's layout and items once Ngó Sen is added, the way install.sh adds it."""
    if "ngosen" in (im for im, _im_layout in items):
        return layout, items
    if len(items) == 1 and items[0][0].startswith("keyboard-"):
        # Ngó Sen switches to English itself, so it takes the lone keyboard's place and layout.
        return items[0][0].removeprefix("keyboard-"), [("ngosen", "")]
    return layout, items + [("ngosen", "")]


def fix_label(action):
    return {
        START_FCITX: _("Start fcitx5"),
        RESTART_FCITX: _("Restart fcitx5"),
        ADD_NGOSEN: _("Add Ngó Sen"),
        ENABLE_FORWARD_KEYS: _("Turn on the fix"),
        INSTALL_INPUT_PANEL: _("Install"),
    }[action]


def fixed_message(action):
    return {
        START_FCITX: _("Started fcitx5."),
        RESTART_FCITX: _("Restarted fcitx5."),
        ADD_NGOSEN: _("Added Ngó Sen to the input method list."),
        ENABLE_FORWARD_KEYS: _("Turned on the fix for GNOME."),
        INSTALL_INPUT_PANEL: _("Installed Input Method Panel."),
    }[action]


# fcitx5 takes a moment to come back on the bus after starting.
SETTLE_MS = {
    START_FCITX: 1500,
    RESTART_FCITX: 1500,
    ENABLE_FORWARD_KEYS: 500,
    INSTALL_INPUT_PANEL: 500,
}
# A slow machine may need longer; check again this often before calling the fix failed.
RETRY_MS, RETRIES = 500, 8


def summary(found, failed=()):
    """Heading and text above the steps; failed fixes have no button left."""
    problems = [f for f in found if f.level != OK]
    if not problems:
        return _("This machine is ready"), _("Try typing in any app.")
    title = _("Steps left to fix: {}").format(len(problems))
    fixable = sum(1 for f in problems if f.action and f.action not in failed)
    if fixable > 1:
        text = _(
            "Use the button on a step, or fix them all at once. Ngó Sen only changes this "
            "account's settings."
        )
    elif fixable:
        text = _("Use the button on the step. Ngó Sen only changes this account's settings.")
    else:
        text = _("Each step says what to do.")
    return title, text


def failed_text(finding):
    if not finding.command:
        return finding.fix
    return _("Could not fix this.") + " " + finding.fix


def run_fix(action, dbus_handler):
    """Runs a step's fix. Returns (done, undo), where undo puts things back or is None."""
    if action == ADD_NGOSEN:
        group = dbus_handler.input_method_group()
        if group is None:
            return False, None
        name, layout, items = group
        if not dbus_handler.set_input_method_group(name, *with_ngosen(layout, items)):
            return False, None
        # fcitx5 quietly drops an input method it has not loaded, such as one installed after it.
        if "ngosen" not in (dbus_handler.input_method_names() or []):
            dbus_handler.set_input_method_group(name, layout, items)
            return False, None
        return True, lambda: dbus_handler.set_input_method_group(name, layout, items)
    if action == RESTART_FCITX:
        return dbus_handler.restart(), None
    if action == START_FCITX:
        return _succeeds(["fcitx5", "-d"]), None
    if action == ENABLE_FORWARD_KEYS:
        if not _succeeds(["gnome-extensions", "enable", FORWARD_KEYS]):
            return False, None
        return True, lambda: _succeeds(["gnome-extensions", "disable", FORWARD_KEYS])
    if action == INSTALL_INPUT_PANEL:
        return _install_input_panel(), None
    return False, None


def _install_input_panel():
    # GNOME asks the user first and answers "cancelled" when they say no; the call waits for them.
    cmd = [
        "gdbus",
        "call",
        "--session",
        "--timeout",
        "120",
        "--dest",
        INPUT_PANEL_INSTALLER,
        "--object-path",
        "/org/gnome/Shell",
        "--method",
        "org.gnome.Shell.Extensions.InstallRemoteExtension",
        INPUT_PANEL,
    ]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=125)
    except (OSError, subprocess.SubprocessError):
        return False
    return out.returncode == 0 and "successful" in out.stdout


def _succeeds(cmd):
    # Not pipes: fcitx5 -d leaves a child holding them, and reading them would wait for it.
    out = subprocess.DEVNULL
    try:
        return subprocess.run(cmd, stdout=out, stderr=out, timeout=10).returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


# Restarting last: a change made over D-Bus is saved when the old fcitx5 exits.
FIX_ORDER = (START_FCITX, ADD_NGOSEN, ENABLE_FORWARD_KEYS, INSTALL_INPUT_PANEL, RESTART_FCITX)


class Fixes:
    """What the check page's fix buttons are doing; the Qt and GTK pages only draw it."""

    def __init__(self, dbus_handler):
        self.dbus_handler = dbus_handler
        self.busy = set()
        self.failed = set()
        self._retries = 0

    def fixable(self, findings):
        return [f.action for f in findings if f.action and f.action not in self.failed]

    def run(self, actions):
        """Runs the fixes; returns (done, undos, ms to wait before checking again)."""
        done, undos = [], {}
        for action in sorted(actions, key=FIX_ORDER.index):
            ok, undo = run_fix(action, self.dbus_handler)
            if not ok:
                self.failed.add(action)
                continue
            done.append(action)
            if undo:
                undos[action] = undo
        return done, undos, max((SETTLE_MS.get(a, 0) for a in done), default=0)

    def settle_again(self, done, findings):
        """ms to wait before checking again while a slow fix may still be taking effect, or 0."""
        left = {f.action for f in findings}
        slow = set(done) & set(SETTLE_MS)
        # A restarted fcitx5 shows as not running until it is back on the bus.
        if slow and left & (slow | {START_FCITX}) and self._retries < RETRIES:
            self._retries += 1
            return RETRY_MS
        self._retries = 0
        return 0

    def finish(self, done, undos, findings):
        """After checking again: the message to show and its undo, empty when nothing got fixed."""
        self.busy = set()
        left = {f.action for f in findings}
        # A fix that ran but left its step, such as a restart KWin ignored, did not work.
        self.failed |= left & set(done)
        self.failed &= left
        fixed = [a for a in done if a not in left]
        if not fixed:
            return "", None
        steps = [undos[a] for a in fixed if a in undos]
        return " ".join(fixed_message(a) for a in fixed), self._undo(steps) if steps else None

    @staticmethod
    def _undo(undos):
        def undo():
            return all([step() for step in reversed(undos)])

        return undo


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
        facts.app_names_running = _shell_names_apps()
        facts.input_panel_state = _extension_state(INPUT_PANEL)
    names = dbus_handler.input_method_names()
    facts.fcitx_running = names is not None
    facts.ngosen_in_group = "ngosen" in (names or [])
    facts.ngosen_loaded = dbus_handler.has_input_method("ngosen") is not False
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


def _shell_names_apps():
    call = ["gdbus", "call", "--session", "--dest", "org.gnome.Shell"]
    call += ["--object-path", "/io/github/ngosen/AppName"]
    call += ["--method", "io.github.ngosen.AppName.Sync"]
    return _output(call).strip() == "()"


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
