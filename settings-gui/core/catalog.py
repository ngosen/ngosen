# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Which settings each page shows, in what order, and the line of help under each."""

from enum import Enum

from i18n import N_


class SettingsCategory(Enum):
    APPEARANCE = "appearance"
    TYPING = "typing"
    SHORTCUTS = "shortcuts"


MODE_SWITCHING = N_("Mode switching")
TRAY_ICON = N_("Tray icon")

# A light and a dark panel, behind the tray icon preview.
TRAY_PANEL = ("#e6e6e6", "#2d2d2d")

# Under "Show in the tray menu" each checkbox names the menu entry and shows its icon.
MENU_ENTRIES = {
    "ShowMenuInputMethod": (N_("Input Method"), "input-keyboard"),
    "ShowMenuCharset": (N_("Charset"), "character-set"),
    "ShowMenuSpellCheck": (N_("Spell Check"), "tools-check-spelling"),
    "ShowMenuMacro": (N_("Macro"), "document-edit"),
    "ShowMenuCapitalizeMacro": (N_("Capitalize Macro"), "format-text-uppercase"),
    "ShowMenuAutoCapitalize": (N_("Auto Capitalize"), "format-text-capitalize"),
    "ShowMenuAutoRestore": (N_("Auto Restore Invalid Words"), "edit-undo"),
    "ShowMenuDictionary": (N_("Custom Dictionary"), "accessories-dictionary"),
}

# Only Breeze has these three; elsewhere (Adwaita, Yaru) the window shows the copies Ngó Sen installs.
MENU_ICON_FALLBACKS = {
    "character-set": "ngosen-charset",
    "format-text-uppercase": "ngosen-capitalize-macro",
    "format-text-capitalize": "ngosen-auto-capitalize",
}

# Groups show in this order; the first checkbox of a group carries the group name.
SETTINGS_MAP = {
    SettingsCategory.TYPING: {
        N_("Typing"): ["Mode", "InputMethod", "OutputCharset"],
        N_("Tone marks"): [
            "SpellCheck",
            "AutoNonVnRestore",
            "LockWordModifier",
            "DdFreeStyle",
            "ModernStyle",
            "FreeMarking",
            "BackspaceRemovesTone",
            "W2U",
            "BracketTransform",
        ],
        N_("Quick typing"): [
            "DoubleSpaceToPeriod",
            "DoubleHyphenToEmDash",
            "AutoCapitalizeAfterPunctuation",
            "CapitalizeStartOfText",
            "RetypeAfterSwitch",
            "RetoneSelection",
        ],
    },
    SettingsCategory.SHORTCUTS: {
        N_("Hotkeys"): ["ToggleKey", "ModeMenuKey", "CycleModeKey"],
        MODE_SWITCHING: [
            "ShortcutSen",
            "ShortcutPreedit",
            "ShortcutEmoji",
            "ShortcutOff",
            "ShortcutDefault",
        ],
    },
    SettingsCategory.APPEARANCE: {
        N_("Language"): ["UiLanguage"],
        TRAY_ICON: ["UseLotusIcons", "IconTheme"],
        N_("Show in the tray menu"): list(MENU_ENTRIES),
        N_("Settings Window"): ["SettingsToolkit"],
    },
}

# One line under a setting, saying what the user sees.
HINTS = {
    "ToggleKey": N_("Tap it alone to switch. The tray icon shows V while typing Vietnamese."),
    "SpellCheck": N_("Words that are not Vietnamese get no tone marks."),
    "AutoNonVnRestore": N_("Typing windows still gives windows."),
    "LockWordModifier": N_(
        "Tap the key alone to keep the word as shown: tunx, Ctrl → tũn, with no restore or macro. Alt also shows the menu bar in some apps."
    ),
    "FreeMarking": N_("Tone marks can also go at the end of the word."),
    "UiLanguage": N_("The tray menu changes at once, this window the next time it opens."),
    "SettingsToolkit": N_(
        "GTK matches GNOME and Cinnamon, Qt matches KDE and most other desktops."
    ),
    "BackspaceRemovesTone": N_(
        "z in Telex (0 in VNI) removes the tone mark even with this off. On a word without one, Backspace deletes as usual."
    ),
    "Mode": N_(
        "Sen types straight into the app. Preedit underlines the word until it is done; use it in an app that Sen does not work with."
    ),
    "OutputCharset": N_(
        "Keep Unicode unless an old document or app needs another one, such as TCVN3 (ABC). Cứu dấu works only with Unicode."
    ),
    "DdFreeStyle": N_("dd and Space stays đ instead of going back to dd."),
    "ModernStyle": N_("hoà, khoẻ, thuý, tuỳ. Off: hòa, khỏe, thúy, tùy."),
    "W2U": N_(
        "Non-Start: nhw → như, but w alone stays w. Everywhere: w alone gives ư too. uw always gives ư."
    ),
    "BracketTransform": N_("ch]a → chưa, m[f → mờ. Type a bracket twice to get it back: [[ → [."),
    "DoubleSpaceToPeriod": N_("ban and Space twice gives “ban. ”."),
    "DoubleHyphenToEmDash": N_("a--b → a—b."),
    "AutoCapitalizeAfterPunctuation": N_("ban. tooi → ban. Tôi. The same after Enter."),
    "CapitalizeStartOfText": N_(
        "xin → Xin in an empty field or on a new line. Not in address bars or some apps; on GNOME 47 and later, Chrome and similar apps skip an empty field."
    ),
    "RetypeAfterSwitch": N_(
        "Typed tieengs vieetj before switching on? Switch on and it becomes tiếng việt. Tab or Esc gives back what you typed."
    ),
    "RetoneSelection": N_(
        "Select nhanh and type s: nhánh. Tab, Esc or s again gives back s. Only the tone mark changes: nguoi does not become người."
    ),
    "ModeMenuKey": N_(
        "Opens a menu to pick a mode for the app in use. Only while typing Vietnamese; in English the key types as usual."
    ),
    "CycleModeKey": N_(
        "Moves the app in use to the next mode in the list below, and remembers it for that app."
    ),
    "ModeOrder": N_(
        "With the mode menu open, press a mode's key to pick it for the app in use. Pick Default Typing to make the app follow the switch key again."
    ),
    "UseLotusIcons": N_("Off: plain one-colour icons, in the colour picked below."),
    "IconTheme": N_(
        "Only for one-colour icons. Light suits a light panel, Dark a dark one, and Auto follows the light or dark setting of the desktop."
    ),
}

MODE_BUTTON_ORDER = ["Sen", "Preedit", "Emoji Picker", "OFF"]

# Each language is named in itself, so it can be found whichever one the window is in.
LANGUAGE_NAMES = {"Vietnamese": "Tiếng Việt", "English": "English"}

# What a word looks like while typing it, shown under the input method.
TYPING_EXAMPLES = {
    "Telex": "vieetj → việt",
    "VNI": "vie65t → việt",
    "VIQR": "vie^.t → việt",
    "Telex + VNI": "vieetj → việt",
    "Telex + VNI + VIQR": "vieetj → việt",
}


MODE_SHORTCUT_TO_VISIBILITY = {
    "ShortcutSen": "ShowModeSen",
    "ShortcutPreedit": "ShowModePreedit",
    "ShortcutEmoji": "ShowModeEmoji",
    "ShortcutOff": "ShowModeOff",
    "ShortcutDefault": "ShowModeDefault",
}

MODE_KEY_TO_INTERNAL_NAME = {
    "ShortcutSen": "Sen",
    "ShortcutPreedit": "Preedit",
    "ShortcutEmoji": "Emoji",
    "ShortcutOff": "Off",
    "ShortcutDefault": "Default",
}

MODE_SHORTCUT_KEYS = list(MODE_SHORTCUT_TO_VISIBILITY.keys())
