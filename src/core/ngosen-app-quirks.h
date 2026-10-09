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
    // input-method frontend, XIM clients, and IBus, D-Bus and fcitx4 clients other than SDL.
    bool forwardsBackspaces(const Field& field);

    // True for GTK4 clients, whose IM modules drop forwarded keys; delete through surrounding text there.
    bool ignoresForwardedKeys(const Field& field);

    // True for Chromium-based browsers on the D-Bus and fcitx4 frontends: they take forwarded keys but
    // report no surrounding text, and their address bar selects an inline autocompletion after the typed text.
    bool selectsOverAutocompletion(const Field& field);

    // Under GNOME, apps reach fcitx5 through GNOME Shell's IBus bridge, which forwards the client's
    // surrounding text tens of milliseconds late and often one edit behind.
    bool surroundingTextLags(const Field& field);

    // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it.
    bool appliesBackspacesLate(const Field& field);

    // LibreOffice Calc over Wayland text-input tells the input method nothing when another cell is
    // selected; it reports the field only when a key reaches it, a lone Shift included.
    bool reportsFieldOnlyOnKey(const Field& field);

    // Firefox's address bar sets no Url flag; callers recognise it by the text after the cursor.
    bool hidesAddressBarFlag(const Field& field);

} // namespace ngosen
