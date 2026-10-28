# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Internationalization setup for the application.
"""

import gettext
import locale
import os
import re

DOMAIN = "fcitx5-ngosen"
LOCALEDIR = "/usr/share/locale"

_translation = gettext.NullTranslations()
_english = {}


def configured_language(env=os.environ):
    """The UiLanguage saved in ngosen.conf; Vietnamese when it was never changed."""
    config_home = env.get("XDG_CONFIG_HOME") or os.path.join(env.get("HOME", ""), ".config")
    # Other keys may hold bytes that are not UTF-8; the one read here is ASCII.
    try:
        with open(
            os.path.join(config_home, "fcitx5", "conf", "ngosen.conf"),
            encoding="utf-8",
            errors="replace",
        ) as f:
            match = re.search(r"^UiLanguage=(\S+)", f.read(), re.MULTILINE)
    except OSError:
        match = None
    return match.group(1) if match else "Vietnamese"


def use_language(language, localedir=LOCALEDIR):
    """Translates into Vietnamese unless English was picked, whatever the desktop language is."""
    global _translation, _english
    vietnamese = gettext.translation(DOMAIN, localedir, languages=["vi"], fallback=True)
    # fcitx5 sends option labels already translated into the language it runs in.
    _english = {
        v: k for k, v in getattr(vietnamese, "_catalog", {}).items() if isinstance(k, str) and k
    }
    _translation = vietnamese if language == "Vietnamese" else gettext.NullTranslations()


def setup_i18n():
    """Initialize gettext in the language picked in the settings."""
    try:
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error as e:
        print(f"Failed to initialize i18n: {e}")
    use_language(configured_language())


def _(text):
    return _translation.gettext(_english.get(text, text))


def N_(text):
    """Marker for strings that should be translated lazily."""
    return text
