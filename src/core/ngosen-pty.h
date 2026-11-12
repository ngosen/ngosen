/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <vector>

#include <termios.h>

namespace ngosen {

    // A password prompt turns echo off but still reads whole lines (sudo, passwd, getpass) or still echoes
    // the newline (sudo-rs). Full-screen programs and shells turn off line reading as well.
    bool hidesInput(const termios& mode);

    // Whether any of these terminals hides input. Ones that are not the user's or cannot be opened are skipped.
    bool anyHidesInput(const std::vector<std::string>& paths);

    // Whether any of the user's pseudo-terminals hides input. Terminals report no pty for their window,
    // so this looks at all of them.
    bool anyPtyHidesInput();

} // namespace ngosen
