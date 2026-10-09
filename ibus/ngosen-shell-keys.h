/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

// The Ngó Sen GNOME Shell extension (gnome-shell/forward-keys@ngosen.github.io) presses keys as a real
// keyboard does. Without XTEST, which Xwayland only allows after asking the user, that is the one way
// to edit text in apps that drop what the input method sends. Both calls fail fast without it.
namespace ngosen::shell {

    // WM_CLASS of the focused window when it is an X11 window; empty otherwise or without the extension.
    std::string focusedX11Class();

    // Presses BackSpace count times in the focused X11 window. False without the extension.
    bool pressBackSpace(int count);

} // namespace ngosen::shell
