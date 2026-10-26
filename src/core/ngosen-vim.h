/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ngosen {

    // What the keys typed so far in Vim's normal mode lead to.
    enum class VimStep : std::uint8_t {
        Pending,     // the command is not finished, or it finished and Vim stays in normal mode
        Insert,      // Vim went into insert mode: the next key is text
        CommandLine, // Vim opened its command line (":", "/", "?"), which takes text until Enter
        Unknown,     // not a Vim command, so the keys were probably text
        Quit,        // ZZ or ZQ: Vim exits
    };

    // Follows a terminal's keys in Vim's normal mode, one command at a time. A terminal reports no
    // text, so this is the only way to tell where a command ends and typed text starts (#302).
    class VimKeyReader {
      public:
        VimStep feed(char key);
        void    reset();

      private:
        std::string pending_;
        bool        visual_ = false;
    };

    // A shell command line that starts Vim, such as "nvim notes.md" or "sudo vim /etc/hosts".
    bool startsVim(std::string_view shellLine);

    // A Vim command line, with its ":", that exits Vim, such as ":wq".
    bool quitsVim(std::string_view commandLine);

} // namespace ngosen
