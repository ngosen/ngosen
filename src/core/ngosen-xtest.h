/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <functional>
#include <string>

// Presses keys through the X server's XTEST extension. They travel like real keys, through the input
// method and then to the focused window, so the backspace bookkeeping applies unchanged. Only on an X11
// session: Xwayland either drops XTEST input or asks the user first.

// True on an X11 session once libxcb-xtest is loaded and the display is reachable.
bool xtestAvailable();

// count > 0 presses BackSpace count times; count < 0 selects -count characters with Shift+Left.
bool xtestSendKeys(int count);

// Tests replace the X server with a recorder; an empty function restores it.
void setXTestSenderForTest(std::function<bool(int)> sender);

// Under Xwayland: presses BackSpace count times as key events sent to the focused X window. Clients
// that read keys through XInput2 (GTK3) ignore them; core-event clients take them like typed keys.
bool xsendAvailable();
bool xsendKeys(int count);
void setXSendSenderForTest(std::function<bool(int)> sender);

// Under Xwayland: the WM_CLASS instance of the focused X window, or "" when there is none.
std::string xFocusedWindowClass();
void        setXFocusedClassForTest(std::function<std::string()> source);
