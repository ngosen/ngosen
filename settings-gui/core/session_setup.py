# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reads and writes what a desktop session needs to start fcitx5, the way install.sh writes it.

On GNOME, install.sh --uninstall removes these files only when they still hold exactly this text,
so the contents here have to stay the same as in install.sh.
"""

import configparser
import glob
import os
import re
import shutil
import subprocess

AUTOSTART_NAME = "org.fcitx.Fcitx5.desktop"
AUTOSTART_SOURCE = f"/usr/share/applications/{AUTOSTART_NAME}"
AUTOSTART_MASK = "[Desktop Entry]\nType=Application\nName=Fcitx 5\nHidden=true\n"
ENV_FILE = "90-ngosen.conf"
GNOME_ENV = "GTK_IM_MODULE=fcitx\nQT_IM_MODULE=fcitx\nXMODIFIERS=@im=fcitx\nSDL_IM_MODULE=fcitx\n"
KDE_ENV = "XMODIFIERS=@im=fcitx\n"
# fcitx5-configtool and fcitx5-remote also contain the name but do not start it.
_RUNS_FCITX5 = re.compile(r"fcitx5(?![\w-])")

ENABLE_AUTOSTART_COMMAND = (
    f"mkdir -p ~/.config/autostart && cp -n {AUTOSTART_SOURCE} ~/.config/autostart/"
)
IM_CONFIG_COMMAND = "im-config -n fcitx5"
KWIN_COMMAND = f"kwriteconfig6 --file kwinrc --group Wayland --key InputMethod {AUTOSTART_SOURCE}"


def set_environment_command(env_text):
    path = f"~/.config/environment.d/{ENV_FILE}"
    text = env_text.replace("\n", "\\n")
    # Appending keeps what the file already holds and still sets these.
    return f"mkdir -p ~/.config/environment.d && printf '{text}' >> {path}"


def _home(env):
    return env.get("HOME") or os.path.expanduser("~")


def _config_home(env):
    return env.get("XDG_CONFIG_HOME") or os.path.join(_home(env), ".config")


def _read(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return None


def _autostart_entries(env):
    """Paths of autostart entries; one in the user's directory hides the system's of that name."""
    dirs = (env.get("XDG_CONFIG_DIRS") or "/etc/xdg").split(":")
    entries = {}
    for directory in [*reversed(dirs), _config_home(env)]:
        for path in glob.glob(os.path.join(glob.escape(directory), "autostart", "*.desktop")):
            entries[os.path.basename(path)] = path
    return entries.values()


def _entry_starts_fcitx5(path):
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        parser.read_string(_read(path) or "")
        section = parser["Desktop Entry"]
    except (configparser.Error, KeyError):
        return False
    if section.get("Hidden", "").lower() == "true":
        return False
    if section.get("X-GNOME-Autostart-enabled", "").lower() == "false":
        return False
    return _RUNS_FCITX5.search(section.get("Exec", "")) is not None


def xinputrc_runs_fcitx5(env):
    text = _read(os.path.join(_home(env), ".xinputrc")) or ""
    return re.search(r"^run_im fcitx5$", text, re.M) is not None


def _im_config_choice(env):
    """What the user picked in ~/.xinputrc, leaving out what only hands the pick back to im-config."""
    choice = re.search(r"^run_im (\S+)$", _read(os.path.join(_home(env), ".xinputrc")) or "", re.M)
    if choice and choice.group(1) not in ("REMOVE", "auto", "default"):
        return choice.group(1)
    return None


def has_im_config():
    return shutil.which("im-config") is not None


def has_kwriteconfig():
    return shutil.which("kwriteconfig6") is not None


def _im_config_output(flag):
    # Untranslated labels, so the line below can be found on any desktop language.
    env = {**os.environ, "LC_ALL": "C"}
    try:
        cmd = ["im-config", flag]
        return subprocess.run(cmd, capture_output=True, text=True, timeout=3, env=env).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def _im_config_default_is_fcitx5():
    """im-config starts fcitx5 by itself when it is the input method it finds on the system."""
    lines = _im_config_output("-m").splitlines()
    if len(lines) > 2 and lines[2] == "fcitx5":
        return True
    # im-config 1.1 replaced -m with -r.
    return re.search(r"^IM framework \(auto\): fcitx5$", _im_config_output("-r"), re.M) is not None


def starts_at_login(env):
    if any(_entry_starts_fcitx5(path) for path in _autostart_entries(env)):
        return True
    # Only im-config reads ~/.xinputrc, and a choice made there beats its default.
    if not has_im_config():
        return False
    choice = _im_config_choice(env)
    if choice:
        return choice == "fcitx5"
    return _im_config_default_is_fcitx5()


def saved_environment(env):
    """Variables from environment.d, which this session may not have read yet; last file wins."""
    saved = {}
    pattern = os.path.join(glob.escape(_config_home(env)), "environment.d", "*.conf")
    for path in sorted(glob.glob(pattern)):
        for name, value in re.findall(r"^(\w+)=(.*)$", _read(path) or "", re.M):
            saved[name] = value.strip().strip("\"'")
    return saved


def _write_new(path, text):
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "x", encoding="utf-8") as f:
            f.write(text)
    except OSError:
        return False
    return True


def _remove(path):
    try:
        os.remove(path)
    except OSError:
        return False
    return True


def _remove_if_holds(path, text):
    return _read(path) == text and _remove(path)


def _succeeds(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, timeout=10).returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def enable_autostart(env):
    """Returns (done, undo). A user entry other than the installer's mask is left alone."""
    path = os.path.join(_config_home(env), "autostart", AUTOSTART_NAME)
    mask = _read(path)
    if mask is not None and (mask.strip() != AUTOSTART_MASK.strip() or not _remove(path)):
        return False, None
    if starts_at_login(env):
        return True, (lambda: _write_new(path, mask)) if mask is not None else None
    source = _read(AUTOSTART_SOURCE)
    if source is None or not _write_new(path, source):
        if mask is not None:
            _write_new(path, mask)
        return False, None

    def undo():
        removed = _remove_if_holds(path, source)
        return removed and (mask is None or _write_new(path, mask))

    return True, undo


def changes_file(env):
    state = env.get("XDG_STATE_HOME") or os.path.join(_home(env), ".local", "state")
    return os.path.join(state, "ngosen", "install-changes")


def _note_change(env, change):
    # install.sh --uninstall takes back only the changes listed here.
    path = changes_file(env)
    if change in (_read(path) or "").splitlines():
        return
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "a", encoding="utf-8") as f:
            f.write(change + "\n")
    except OSError:
        pass


def _reload_user_environment():
    # The systemd user manager can outlive a logout and would keep the old variables.
    _succeeds(["systemctl", "--user", "daemon-reload"])


def set_environment(env):
    """Returns (done, undo). A value the user set in environment.d stays theirs."""
    env_text = KDE_ENV if "KDE" in env.get("XDG_CURRENT_DESKTOP", "").upper() else GNOME_ENV
    path = os.path.join(_config_home(env), "environment.d", ENV_FILE)
    saved = saved_environment(env)
    wanted = dict(line.split("=", 1) for line in env_text.splitlines())
    if any(saved.get(name, value) != value for name, value in wanted.items()):
        return False, None
    if not _write_new(path, env_text):
        return False, None
    _reload_user_environment()
    undo_im_config = None
    # Debian's im-config otherwise picks IBus for X11 sessions and overrides these variables.
    if env_text == GNOME_ENV and has_im_config() and _im_config_choice(env) is None:
        _done, undo_im_config = use_im_config(env)

    def undo():
        removed = _remove_if_holds(path, env_text)
        _reload_user_environment()
        return removed and (undo_im_config is None or undo_im_config())

    return True, undo


def use_im_config(env):
    """Returns (done, undo). im-config sets the variables and starts fcitx5 at login."""
    path = os.path.join(_home(env), ".xinputrc")
    before = _read(path)
    # im-config 1.1 replaced -n with -w.
    if not _succeeds(["im-config", "-n", "fcitx5"]) and not _succeeds(
        ["im-config", "-w", "fcitx5"]
    ):
        return False, None
    _note_change(env, "im-config")

    def undo():
        if before is None:
            return _remove(path)
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(before)
        except OSError:
            return False
        return True

    return True, undo


def set_kwin_input_method(previous):
    """Returns (done, undo). KWin reads this at login; only an fcitx5 it starts serves Qt apps.

    An input method the user chose stays theirs.
    """
    if previous or not os.path.exists(AUTOSTART_SOURCE):
        return False, None
    key = ["kwriteconfig6", "--file", "kwinrc", "--group", "Wayland", "--key", "InputMethod"]
    if not _succeeds([*key, AUTOSTART_SOURCE]):
        return False, None
    return True, lambda: _succeeds([*key, previous])
