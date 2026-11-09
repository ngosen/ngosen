# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tells from the package manager's files whether this machine gets new Ngó Sen versions.

Only files are read: the check page runs on the UI thread, and a package manager can take seconds.
"""

import configparser
import glob
import os
import re

ON, OFF, UNKNOWN = "on", "off", "unknown"
# Nix and other installs that no repository updates; the page then shows only the version.
NONE = ""

APT_URL = "https://ngosen.github.io/ngosen/apt"
COPR_URL = "copr.fedorainfracloud.org/results/nguyenphi/ngosen"
ZYPPER_URL = "ngosen.github.io/ngosen/opensuse"
PACMAN_SECTION = "ngosen"

_APT_DISABLED = re.compile(r"(no|false|off|without|disable|0)$")
# apt skips files with other characters in their names.
_APT_FILE_NAME = re.compile(r"[A-Za-z0-9_.-]+$")
_RPM_DISABLED = ("0", "false", "no", "off")


def _same_uri(uri, want):
    return uri.rstrip("/") == want.rstrip("/")


def apt_list_enables(text, want=APT_URL):
    """One-line format, as install.sh reads it: a deb line, options in brackets skipped."""
    for line in text.splitlines():
        words = line.split()
        if not words or words[0] != "deb":
            continue
        i = 1
        if i < len(words) and words[i].startswith("["):
            while i < len(words) - 1 and not words[i].endswith("]"):
                i += 1
            i += 1
        if i < len(words) and _same_uri(words[i], want):
            return True
    return False


def _deb822_stanza_enables(fields, want):
    if _APT_DISABLED.match(fields.get("enabled", "").strip().lower()):
        return False
    if "deb" not in fields.get("types", "").split():
        return False
    return any(_same_uri(uri, want) for uri in fields.get("uris", "").split())


def apt_sources_enables(text, want=APT_URL):
    """deb822 format (.sources), stanza by stanza."""
    fields, key = {}, ""
    # A commented-out field reads as a key such as "# uris", which nothing looks up.
    for line in text.splitlines() + [""]:
        if not line.strip():
            if _deb822_stanza_enables(fields, want):
                return True
            fields, key = {}, ""
        elif line[0] in " \t":
            if key:
                fields[key] += " " + line
        elif ":" in line:
            key, _, value = line.partition(":")
            key = key.lower()
            fields[key] = value
    return False


def rpm_repo_enables(text, url):
    """A dnf or zypper .repo file; a section without enabled= is on."""
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        parser.read_string(text)
    except configparser.Error:
        return False
    for section in parser.sections():
        repo = parser[section]
        if (
            url in repo.get("baseurl", "")
            and repo.get("enabled", "1").strip().lower() not in _RPM_DISABLED
        ):
            return True
    return False


def pacman_sections(text):
    """Section names that are not commented out, and the files included at the top level."""
    sections, includes = [], []
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            sections.append(line[1:-1].strip())
        elif "=" in line and not line.startswith("#"):
            key, _, value = line.partition("=")
            if key.strip() == "Include":
                includes.append(value.strip())
    return sections, includes


def _read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def _apt_files(root):
    etc = os.path.join(root, "etc/apt")
    files = [os.path.join(etc, "sources.list")]
    files += sorted(glob.glob(os.path.join(etc, "sources.list.d/*.list")))
    files += sorted(glob.glob(os.path.join(etc, "sources.list.d/*.sources")))
    return [f for f in files if _APT_FILE_NAME.match(os.path.basename(f))]


def _apt_enables(path, text):
    if path.endswith(".sources"):
        return apt_sources_enables(text)
    return apt_list_enables(text)


def _candidates(root):
    """(path, does this text enable the repository) for every file a package manager reads."""
    for path in _apt_files(root):
        yield path, lambda text, path=path: _apt_enables(path, text)
    for path in sorted(glob.glob(os.path.join(root, "etc/yum.repos.d/*.repo"))):
        yield path, lambda text: rpm_repo_enables(text, COPR_URL)
    for path in sorted(glob.glob(os.path.join(root, "etc/zypp/repos.d/*.repo"))):
        yield path, lambda text: rpm_repo_enables(text, ZYPPER_URL)


def _pacman_state(root):
    conf = os.path.join(root, "etc/pacman.conf")
    if not os.path.exists(conf):
        return OFF
    try:
        sections, includes = pacman_sections(_read(conf))
    except OSError:
        return UNKNOWN
    if PACMAN_SECTION in sections:
        return ON
    state = OFF
    for include in includes:
        for path in glob.glob(os.path.join(root, include.lstrip("/"))):
            try:
                if PACMAN_SECTION in pacman_sections(_read(path))[0]:
                    return ON
            except OSError:
                state = UNKNOWN
    return state


def state(root="/", program=__file__):
    """ON as soon as one file enables the repository, even when another could not be read."""
    if os.path.realpath(program).startswith("/nix/store/"):
        return NONE
    found = _pacman_state(root)
    if found == ON:
        return ON
    for path, enables in _candidates(root):
        if not os.path.exists(path):
            continue
        try:
            if enables(_read(path)):
                return ON
        except OSError:
            found = UNKNOWN
    return found
