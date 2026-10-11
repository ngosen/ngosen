# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Helper utilities and shared mappings for the Ngó Sen settings window.
"""

from i18n import N_, _

from ui.components import HelpIcon

# Tooltip text for specific settings keys
HELPERS = {
    "ShortcutSen": N_("Text goes straight into the field, without an underline."),
    "CapitalizeMacro": N_(
        "Automatically match expansion case to trigger key case.\n\n"
        "Example if 'kg' is 'Khô gà':\n"
        "- kg -> khô gà\n"
        "- Kg -> Khô gà\n"
        "- KG -> KHÔ GÀ"
    ),
    "EnableMacroInOffMode": N_(
        "Allow macros to work when the input mode is OFF.\n"
        "When disabled, macros are only available in active typing modes."
    ),
    "MacroSkipTriggerModifier": N_(
        "Press and release the selected modifier key before typing to skip macro expansion for the next word.\n"
        "If another key is pressed between the modifier's press and release, the skip is cancelled."
    ),
}


def add_help_icon(layout, key, clear_existing=False):
    """
    Utility to add a HelpIcon to a layout based on a setting key.
    Optionally clears existing HelpIcons from the layout first.
    """
    if clear_existing:
        # Avoid duplicate icons by removing existing ones in the layout cleanly
        for i in reversed(range(layout.count())):
            item = layout.itemAt(i)
            if item and item.widget() and isinstance(item.widget(), HelpIcon):
                widget = layout.takeAt(i).widget()
                if widget:
                    widget.deleteLater()

    # Only add if we have a mapped helper text
    helper_text = HELPERS.get(key)
    if helper_text:
        icon = HelpIcon(_(helper_text))
        layout.addWidget(icon)
        return icon
    return None
