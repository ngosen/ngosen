/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-app-quirks.h"

#include "ngosen-strings.h"
#include "ngosen-xtest.h"

namespace ngosen {

    namespace {
        bool isChromiumBrowser(const std::string& program) {
            for (const char* browser : {"chromium", "chrome", "google-chrome", "microsoft-edge", "msedge", "brave", "opera"}) {
                if (isStartsWith(program, browser))
                    return true;
            }
            return false;
        }

        // The app's own program name. IBus names a client "<toolkit>-im:<program>" (gtk3-im:firefox), and a
        // snap's program repeats the snap name (firefox_firefox).
        std::string appName(const Field& field) {
            std::string program = field.program;
            if (field.frontend == "ibus") {
                const size_t colon = program.find(':');
                if (colon != std::string::npos && isEndsWith(program.substr(0, colon), "-im"))
                    program.erase(0, colon + 1);
                const size_t underscore = program.find('_');
                if (underscore != std::string::npos && program.compare(0, underscore, program, underscore + 1) == 0)
                    program.erase(underscore);
            }
            return stripDesktopSuffix(program);
        }
    } // namespace

    bool forwardsBackspaces(const Field& field) {
        if (field.sendsNoKeys)
            return false;
        // On wayland_v2 (input-method-v2: Sway, Hyprland) fcitx5 sends forwarded keys through a virtual keyboard.
        if (field.frontend == "wayland" || field.frontend == "wayland_v2") {
            return true;
        }
        if (field.frontend == "xim") {
            // A forwarded key that reaches a client inside one of its own XIM requests comes back to us
            // and lands after the commit (WordCraft); XTEST keys queue in order like typed ones.
            return !xtestAvailable() && !takesSentKeyEvents(field);
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

    bool takesSentKeyEvents(const Field& field) {
        if (field.frontend != "xim" || !xsendAvailable())
            return false;
        // Wine applies forwarded keys after a later commit and reports nothing that sets it apart;
        // xterm and XInput2 clients drop sent events, so the rest keep forwarding.
        return isStartsWith(appName(field), "wine");
    }

    bool ignoresForwardedKeys(const Field& field) {
        if (field.frontend == "ibus") {
            // IBus for GTK4 feeds forwarded keys back into the IM instead of the widget.
            return isStartsWith(field.program, "gtk4-im:");
        }
        // fcitx5-gtk4 discards forwarded keys, and is the D-Bus client that leaves this flag unset.
        return field.frontend == "dbus" && !field.keyEventOrderFix && field.surroundingText;
    }

    bool appliesEarlyKeysFirst(const Field& field) {
        // Firefox over D-Bus does this; other D-Bus clients keep the order, so a commit costs them nothing.
        return field.frontend == "dbus" && field.surroundingText;
    }

    bool erasesWithCommittedDel(const Field& field) {
        if (field.surroundingText)
            return false;
        if (field.terminal)
            return true;
        const bool gtk4Module = (field.frontend == "dbus" && !field.keyEventOrderFix) || (field.frontend == "ibus" && isStartsWith(field.program, "gtk4-im:"));
        if (!gtk4Module)
            return false;
        // Neither GTK4 IM module passes on the terminal input purpose VTE sets; these are the GTK4 VTE terminals.
        const std::string program = appName(field);
        return program == "ptyxis" || program == "kgx";
    }

    bool selectsOverAutocompletion(const Field& field) {
        if ((field.frontend != "dbus" && field.frontend != "fcitx4") || !forwardsBackspaces(field) || field.surroundingText || field.caretWidth <= 0)
            return false;
        const std::string program = appName(field);
        // Vivaldi draws its address bar as a web page field, so only the selected autocompletion sets it
        // apart from a zero-width page caret.
        if (isStartsWith(program, "vivaldi"))
            return true;
        // Chromium reports a thin caret for its own text fields. A web page field with a selection reports
        // the selection instead (Google Sheets autocompletion), and Shift+Left there loses text.
        if (field.caretWidth * 8 > field.caretHeight)
            return false;
        // Terminals match the checks above too, and print Shift+Left instead of selecting.
        return isChromiumBrowser(program);
    }

    bool typesOverPrimarySelection(const Field& field) {
        // Terminals, Emacs and Vim also own the primary selection and report the same, but type elsewhere.
        const std::string program = appName(field);
        // WPS Writer, Spreadsheets and Presentation.
        const bool wps = program == "wps" || program == "et" || program == "wpp";
        return !field.surroundingText && (isChromiumBrowser(program) || isStartsWith(program, "vivaldi") || isStartsWith(program, "coccoc") || wps);
    }

    bool surroundingTextLags(const Field& field) {
        return field.frontend == "ibus";
    }

    // Only GNOME Shell sends edits through mutter; nothing else the field reports tells it apart.
    bool keepsLastEditPerTurn(const Field& field) {
        return field.frontend == "ibus" && appName(field) == "gnome-shell";
    }

    bool dropsCommitsWithoutFocus(const Field& field) {
        return field.frontend == "wayland" || field.frontend == "wayland_v2";
    }

    bool appliesBackspacesLate(const Field& field) {
        return appName(field) == "soffice";
    }

    bool deletesThroughSurroundingFirst(const Field& field) {
        // XIM has no surrounding text; fcitx4 is the GTK module in old snaps, which nobody has measured.
        return field.frontend == "ibus" || field.frontend == "wayland" || field.frontend == "wayland_v2" || field.frontend == "dbus";
    }

    bool reportsSurroundingEditsLate(const Field& field) {
        // Firefox may report our deletion and commit only after the next key, and the replacement that
        // finds the copy stale then mixes both paths. Nothing it reports shows this before it happens.
        // Over IBus the wait for each key's report covers it.
        return field.frontend != "ibus" && hidesAddressBarFlag(field);
    }

    bool countsDeletionOnCopy(const Field& field) {
        return field.frontend == "ibus" || field.frontend == "wayland" || field.frontend == "wayland_v2";
    }

    bool reportsFieldOnlyOnKey(const Field& field) {
        // Nothing Calc reports sets it apart from a text field that keeps its text.
        return field.frontend == "wayland" && appName(field) == "libreoffice-calc";
    }

    bool hidesAddressBarFlag(const Field& field) {
        // Debian names its Firefox ESR program firefox-esr; the forks share Firefox's address bar.
        const std::string program = appName(field);
        if (isStartsWith(program, "firefox"))
            return true;
        for (const char* fork : {"zen", "librewolf", "waterfox", "floorp", "mullvadbrowser", "icecat"}) {
            if (program == fork)
                return true;
        }
        return false;
    }

} // namespace ngosen
