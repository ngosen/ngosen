/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <functional>

// Presses keys through the X server's XTEST extension. They travel like real keys, through the input
// method and then to the focused window, so the uinput bookkeeping applies unchanged. Only on an X11
// session: Xwayland either drops XTEST input or asks the user first.

// True on an X11 session once libxcb-xtest is loaded and the display is reachable.
bool xtestAvailable();

// count > 0 presses BackSpace count times; count < 0 selects -count characters with Shift+Left.
bool xtestSendKeys(int count);

// Tests replace the X server with a recorder; an empty function restores it.
void setXTestSenderForTest(std::function<bool(int)> sender);
