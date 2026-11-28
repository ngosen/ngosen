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
    CONVERT = "convert"
    EMOJI = "emoji"
    GUESSING = "guessing"


TYPING_GROUP = N_("Typing")
TYPING_KEYS = N_("Typing keys")
MODE_SWITCHING = N_("Mode switching")
TRAY_ICON = N_("Tray icon")
CONVERT_CHARSETS = N_("Charsets")
EMOJI_GROUP = N_("Emoji")
LEARNING_GROUP = N_("Learning")
# Restoring defaults leaves these as the user set them, since they decide what is kept about them.
KEPT_ON_RESET = ("LearnDiacritics",)

# A light and a dark panel, behind the tray icon preview.
TRAY_PANEL = ("#e6e6e6", "#2d2d2d")

# Under "Show in the tray menu" each checkbox names the menu entry and shows its icon.
MENU_ENTRIES = {
    "ShowMenuInputMethod": (N_("Input Method"), "input-keyboard"),
    "ShowMenuCharset": (N_("Charset"), "character-set"),
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

# A page whose title is not its category's name.
PAGE_TITLES = {SettingsCategory.GUESSING: N_("Word guessing")}

# Groups show in this order; the first checkbox of a group carries the group name.
SETTINGS_MAP = {
    SettingsCategory.TYPING: {
        TYPING_GROUP: ["Mode", "InputMethod", "OutputCharset"],
        # The switch others depend on comes first.
        N_("Fixing words"): [
            "AutoNonVnRestore",
            "DdFreeStyle",
            "RetypeAfterSwitch",
            "RetoneSelection",
        ],
        N_("Tone marks"): ["ModernStyle", "FreeMarking", "BackspaceRemovesTone"],
        TYPING_KEYS: ["W2U", "BracketTransform", "LockWordModifier"],
        N_("Quick typing"): [
            "DoubleSpaceToPeriod",
            "DoubleHyphenToEmDash",
            "AutoCapitalizeAfterPunctuation",
            "CapitalizeStartOfText",
            "RawWordAfterSlash",
        ],
    },
    SettingsCategory.GUESSING: {
        N_("Adding marks"): [
            "GuessDiacritics",
            "GuessFixesTypos",
            "GuessFixStrength",
            "GuessInTerminals",
        ],
        LEARNING_GROUP: ["LearnDiacritics"],
    },
    SettingsCategory.EMOJI: {
        EMOJI_GROUP: [
            "EmojiSuggestions",
            "EmojiOfferAt",
            "EmojiPlacement",
            "YahooEmoticons",
            "EnglishEmojiKeywords",
        ],
    },
    SettingsCategory.SHORTCUTS: {
        N_("Hotkeys"): ["ToggleKey", "ToggleScope", "ModeMenuKey", "CycleModeKey"],
        MODE_SWITCHING: [
            "ShortcutSen",
            "ShortcutPreedit",
            "ShortcutEmoji",
            "ShortcutTeencode",
            "ShortcutChatShorthand",
            "ShortcutOff",
            "ShortcutVim",
        ],
    },
    SettingsCategory.CONVERT: {
        N_("Hotkey"): ["ConvertKey"],
        CONVERT_CHARSETS: ["ConvertFrom", "ConvertTo"],
        N_("After converting"): ["ConvertStripDiacritics", "ConvertCase"],
    },
    SettingsCategory.APPEARANCE: {
        N_("Language"): ["UiLanguage"],
        TRAY_ICON: [
            "TrayIconKind",
            "TrayImageVietnamese",
            "TrayImageEnglish",
            "TrayIconSize",
            "IconTheme",
        ],
        N_("Show in the tray menu"): list(MENU_ENTRIES),
        N_("Settings Window"): ["SettingsToolkit"],
    },
}

# These groups have no checkbox to carry their name, so the Qt window gives it a line of its own.
GROUPS_WITH_NAME_LINE = {TYPING_KEYS, TRAY_ICON}

# One line under a setting, saying what the user sees.
HINTS = {
    "ToggleKey": N_("Tap it alone to switch. The tray icon shows V while typing Vietnamese."),
    "ToggleScope": N_(
        "All apps switches English/Vietnamese everywhere except apps set to OFF; Each app remembers a separate choice for each app."
    ),
    "CapitalizeMacro": N_("kg gives khô gà, Kg gives Khô gà, KG gives KHÔ GÀ."),
    "EnableMacroInOffMode": N_("Abbreviations also expand while the mode is Off."),
    "ChatShorthand": N_(
        "ko gives không, dc gives được. Backspace right after the space gives back what you typed."
    ),
    "AutoNonVnRestore": N_("Typing windows still gives windows."),
    "LockWordModifier": N_(
        "Tap the key alone to keep the word as shown: tunx, Ctrl → tũn, with no restore or macro. Alt also shows the menu bar in some apps."
    ),
    "FreeMarking": N_("Tone marks can also go at the end of the word."),
    "UiLanguage": N_("The tray menu changes at once, this window the next time it opens."),
    "SettingsToolkit": N_(
        "GTK matches GNOME, Cinnamon and Xfce, Qt matches KDE and most other desktops."
    ),
    "BackspaceRemovesTone": N_(
        "z in Telex (0 in VNI) removes the tone mark even with this off. On a word without one, Backspace deletes as usual."
    ),
    "OutputCharset": N_(
        "Keep Unicode unless an old document or app needs another one, such as TCVN3 (ABC). "
        "Unicode tổ hợp sends tone marks apart from their letters, which terminals and many apps "
        "handle badly. Cứu dấu works only with Unicode."
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
        "Select nhanh and type s: nhánh. s again gives back s, as do Tab and Esc in Sen. Only the tone mark changes: nguoi does not become người."
    ),
    "EmojiSuggestions": N_(
        "cười shows Tab: 😂 … next to the cursor. Tab numbers them: 1–5 picks one, Tab, Space or Enter the first, Esc keeps the word. haha and :) are replaced. Only where the hint is known to show."
    ),
    "YahooEmoticons": N_(
        "=)) and :x offer 🤣 and 😍, like on Yahoo Messenger; Tab swaps the code for the emoji."
    ),
    "EnglishEmojiKeywords": N_(
        "cat and thumbs up also offer 🐈 and 👍, from the Unicode list of English words. Words that"
        " are also Vietnamese, like can, are left out."
    ),
    "EmojiOfferAt": N_(
        "At the end of a sentence: the emoji of the sentence show while you type. After . ! ? or a"
        " short pause, press Tab, then 1–9 to pick one; it goes at the cursor. Esc hides them until"
        " the next sentence."
        " After each word: the list shows as soon as a keyword is typed."
    ),
    "EmojiPlacement": N_(
        "After the word: cười 😂. Instead of the word: 😂. haha and :) are always replaced."
    ),
    "GuessDiacritics": N_(
        "Type toi di hoc and get tôi đi học; a word can change again as the next ones come. Backspace right after a word puts it back as typed. Never in password fields."
    ),
    "GuessFixesTypos": N_("phmi becomes phím. Off, a syllable no word matches stays as typed."),
    "GuessFixStrength": N_(
        "Stronger fixes more typos, and now and then changes a word that was right. A new level"
        " applies from the next thing you type."
    ),
    "GuessInTerminals": N_(
        "A terminal does not let Ngó Sen read back what you typed, so guesses there are wrong more"
        " often and Backspace cannot bring back what you typed."
    ),
    "LearnDiacritics": N_(
        "Words you type with marks are kept on this computer so guesses fit how you write. Never"
        " in password fields, and never sent anywhere."
    ),
    "RawWordAfterSlash": N_(
        "/cost and /usr/share stay as typed, at the start of a line or after a space, so chat commands and file paths work. và/hoặc and @tên still get tones."
    ),
    "ModeMenuKey": N_(
        "Opens a menu to pick a mode for the app in use. Only while typing Vietnamese; in English the key types as usual."
    ),
    "ConvertKey": N_(
        "Select text in an app, then press this key. The text changes in place; Ctrl+Z brings it back."
    ),
    "ConvertFrom": N_("Old documents are often TCVN3."),
    "ConvertStripDiacritics": N_("Tiếng Việt becomes Tieng Viet."),
    "CycleModeKey": N_(
        "Moves the app in use to the next mode in the list below, and remembers it for that app. "
        "With All apps, choosing Sen or Preedit while in English turns Vietnamese on for all apps."
    ),
    "ModeOrder": N_(
        "With the mode menu open, press a mode's key to pick it for the app in use. Pick the app's default mode (usually Sen, sometimes Preedit) "
        "to follow the global setting and switch key again; OFF stays set for the app and keeps it in English even when other apps type Vietnamese. "
        "With All apps, the switch key in an OFF app opens the mode menu; pick Sen or Preedit there to type Vietnamese in that app."
    ),
    "TrayIconKind": N_("V / E and vi / en take the colour picked below."),
    "TrayImageVietnamese": N_("Square SVG, or PNG from 64×64, on a transparent background."),
    "TrayIconSize": N_(
        "Large draws the Ngó Sen icon bigger in its tray slot. No change on GNOME, where it already fills the slot."
    ),
    "IconTheme": N_(
        "Light suits a light panel, Dark a dark one; Auto follows the light or dark setting of the desktop."
    ),
}

MODE_BUTTON_ORDER = ["Sen", "Preedit"]
MODE_HINTS = {
    "Sen": N_("Text goes straight into the app as you type."),
    "Preedit": N_(
        "The word is underlined until it is done. Use it in an app that Sen does not work with."
    ),
}


def shown_mode(saved):
    """The switch key picks English, so a saved OFF types as Sen."""
    return "Sen" if saved == "OFF" else saved


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
    "ShortcutTeencode": "ShowModeTeencode",
    "ShortcutChatShorthand": "ShowModeChatShorthand",
    "ShortcutOff": "ShowModeOff",
    "ShortcutVim": "ShowModeVim",
}


def mode_order(saved):
    """The mode menu order as the window shows it: known modes only, missing ones at the end."""
    known = list(MODE_KEY_TO_INTERNAL_NAME.values())
    # An order saved before a mode was dropped still names it.
    order = [n for n in dict.fromkeys(str(saved).split(",")) if n in known]
    return order + [n for n in known if n not in order]


MODE_KEY_TO_INTERNAL_NAME = {
    "ShortcutSen": "Sen",
    "ShortcutPreedit": "Preedit",
    "ShortcutEmoji": "Emoji",
    "ShortcutTeencode": "Teencode",
    "ShortcutChatShorthand": "ChatShorthand",
    "ShortcutOff": "Off",
    "ShortcutVim": "Vim",
}

MODE_SHORTCUT_KEYS = list(MODE_SHORTCUT_TO_VISIBILITY.keys())

# Settings that do nothing unless another one, shown next to them, holds this value.
DEPENDS = {
    "DdFreeStyle": ("AutoNonVnRestore", "True"),
    "GuessFixesTypos": ("GuessDiacritics", "True"),
    "GuessFixStrength": ("GuessFixesTypos", "True"),
    "GuessInTerminals": ("GuessDiacritics", "True"),
    "LearnDiacritics": ("GuessDiacritics", "True"),
    "IconTheme": ("TrayIconKind", ("V / E", "vi / en")),
    "TrayIconSize": ("TrayIconKind", "Colour icon"),
    "TrayImageVietnamese": ("TrayIconKind", "Custom image"),
    "TrayImageEnglish": ("TrayIconKind", "Custom image"),
    "CapitalizeMacro": ("EnableMacro", "True"),
    "EnableMacroInOffMode": ("EnableMacro", "True"),
    "TimeFormat": ("EnableMacro", "True"),
    "YahooEmoticons": ("EmojiSuggestions", "True"),
    "EmojiPlacement": ("EmojiSuggestions", "True"),
    "EmojiOfferAt": ("EmojiSuggestions", "True"),
    "EnglishEmojiKeywords": ("EmojiSuggestions", "True"),
    "DateFormat": ("EnableMacro", "True"),
    **{shortcut: (shown, "True") for shortcut, shown in MODE_SHORTCUT_TO_VISIBILITY.items()},
}


# A second value that also makes a setting count, next to the one in DEPENDS.
# fcitx5 stores TrayIconKind by the name it shows; these name the letters each one draws.
TRAY_LETTERS = {
    "V / E": ("fcitx-ngosen", "fcitx-ngosen-off"),
    "vi / en": ("fcitx-ngosen-text-vi", "fcitx-ngosen-text-en"),
}

TRAY_IMAGE_KEYS = {"TrayImageVietnamese": "vi", "TrayImageEnglish": "en"}
# Screen readers hear these, as both rows have the same two buttons.
TRAY_IMAGE_BUTTONS = {
    "TrayImageVietnamese": (
        N_("Choose the tray image for Vietnamese"),
        N_("Remove the tray image for Vietnamese"),
    ),
    "TrayImageEnglish": (
        N_("Choose the tray image for English"),
        N_("Remove the tray image for English"),
    ),
}
TRAY_KEYS = ("TrayIconKind", *TRAY_IMAGE_KEYS, "TrayIconSize", "IconTheme")


def _holds(controller, needed, value_of):
    value = value_of(controller)
    if value is None:
        return True
    allowed = (needed,) if isinstance(needed, str) else needed
    return str(value).lower() in (v.lower() for v in allowed)


def locked_by(key, value_of):
    """The setting that keeps key from doing anything right now, or None.

    value_of(name) gives a setting's value, or None when fcitx5 does not report that setting.
    """
    controller, needed = DEPENDS.get(key, (None, None))
    if controller is None:
        return None
    # A controller locked itself cannot be changed, so what locks it comes first.
    return locked_by(controller, value_of) or (
        None if _holds(controller, needed, value_of) else controller
    )
