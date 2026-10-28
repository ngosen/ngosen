/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"

// How apps differ in taking text and deletions from the input method. Decided from what the field
// reports where possible; a program name is matched only when nothing reported tells the app apart.
namespace ngosen {

    // True when the frontend can deliver backspaces itself, so XTEST is not needed: the Wayland
    // input-method frontend, XIM clients under Xwayland, and IBus, D-Bus and fcitx4 clients other than SDL.
    bool forwardsBackspaces(const Field& field);

    // True for Wine over XIM under Xwayland, where XTEST asks the user first: backspaces go
    // as key events sent to the focused X window instead.
    bool takesSentKeyEvents(const Field& field);

    // True for GTK4 clients, whose IM modules drop forwarded keys; delete through surrounding text there.
    bool ignoresForwardedKeys(const Field& field);

    // True when a key the app sent before our commit reached it can land ahead of that commit.
    bool appliesEarlyKeysFirst(const Field& field);

    // True for VTE terminals under GTK4 (Ptyxis, GNOME Console), through fcitx5-gtk4 or IBus: they take neither
    // forwarded keys nor surrounding text, but pass committed text to the shell as is, so a DEL in it erases like
    // Backspace.
    bool erasesWithCommittedDel(const Field& field);

    // True for the address bar of Chromium-based browsers on the D-Bus and fcitx4 frontends: it reports no
    // surrounding text and selects an inline autocompletion after the typed text.
    bool selectsOverAutocompletion(const Field& field);

    // True for Chromium-based browsers that report no surrounding text, as on X11: a key typed there replaces
    // the selection, which is also the X11 primary selection.
    bool typesOverPrimarySelection(const Field& field);

    // Under GNOME, apps reach fcitx5 through GNOME Shell's IBus bridge, which can relay the client's
    // surrounding text tens of milliseconds late, one edit at a time.
    bool surroundingTextLags(const Field& field);

    // Wayland text input delivers a commit only to the field that has focus.
    bool dropsCommitsWithoutFocus(const Field& field);

    // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it.
    bool appliesBackspacesLate(const Field& field);

    // LibreOffice Calc over Wayland text-input tells the input method nothing when another cell is
    // selected; it reports the field only when a key reaches it, a lone Shift included.
    bool reportsFieldOnlyOnKey(const Field& field);

    // The address bar of Firefox and its forks sets no Url flag; callers recognise it by the text after the cursor.
    bool hidesAddressBarFlag(const Field& field);

} // namespace ngosen
