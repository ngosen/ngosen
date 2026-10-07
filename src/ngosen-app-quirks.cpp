/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-app-quirks.h"

#include "lotus-utils.h"

namespace ngosen {

    bool forwardsBackspaces(const Field& field) {
        if (field.frontend == "wayland" || field.frontend == "xim") {
            return true;
        }
        if (field.frontend == "ibus") {
            // SDL takes only commits and preedit from the IM.
            return !isStartsWith(field.program, "SDL");
        }
        if (field.frontend == "dbus") {
            // fcitx5-gtk and fcitx5-qt set one of these; SDL sets neither and handles only commits and preedit.
            return field.keyEventOrderFix || field.surroundingText;
        }
        if (field.frontend == "fcitx4") {
            // Snap apps bundle the fcitx4 GTK module, which sets one of these; SDL 2.0.12 and older speak
            // this protocol too and set at most Preedit.
            return field.formattedPreedit || field.surroundingText;
        }
        return false;
    }

    bool ignoresForwardedKeys(const Field& field) {
        if (field.frontend == "ibus") {
            // IBus for GTK4 feeds forwarded keys back into the IM instead of the widget.
            return isStartsWith(field.program, "gtk4-im:");
        }
        // fcitx5-gtk4 discards forwarded keys, and is the D-Bus client that leaves this flag unset.
        return field.frontend == "dbus" && !field.keyEventOrderFix && field.surroundingText;
    }

    bool selectsOverAutocompletion(const Field& field) {
        if ((field.frontend != "dbus" && field.frontend != "fcitx4") || !forwardsBackspaces(field) || field.surroundingText)
            return false;
        // Terminals match the checks above too, and print Shift+Left instead of selecting.
        const std::string program = stripDesktopSuffix(field.program);
        for (const char* browser : {"chromium", "chrome", "google-chrome", "microsoft-edge", "msedge", "brave", "vivaldi", "opera"}) {
            if (isStartsWith(program, browser))
                return true;
        }
        return false;
    }

    bool surroundingTextLags(const Field& field) {
        return field.frontend == "ibus";
    }

    bool appliesBackspacesLate(const Field& field) {
        return stripDesktopSuffix(field.program) == "soffice";
    }

    bool hidesAddressBarFlag(const Field& field) {
        return stripDesktopSuffix(field.program) == "firefox";
    }

} // namespace ngosen
