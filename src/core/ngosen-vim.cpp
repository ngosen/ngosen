/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-vim.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace ngosen {

    namespace {

        enum class Token : std::uint8_t {
            Incomplete,
            Normal,
            Insert,
            CommandLine,
            VisualStart,
            VisualEnd,
            Quit,
            Unknown
        };

        constexpr std::string_view simpleMotions = "hjklwWbBeE0^$GHML{}()|_+-;,nN% ";
        constexpr std::string_view charMotions   = "fFtT`'";
        constexpr std::string_view textObjects   = "wWsp()bB[]{}<>t\"'`";

        bool                       has(std::string_view set, char c) {
            return set.find(c) != std::string_view::npos;
        }

        bool isDigit(char c) {
            return c >= '0' && c <= '9';
        }

        // Skips a count such as the 3 of "3w"; a lone 0 is the motion to the line start.
        std::string_view withoutCount(std::string_view keys) {
            size_t i = 0;
            if (!keys.empty() && keys[0] >= '1' && keys[0] <= '9') {
                while (i < keys.size() && isDigit(keys[i]))
                    ++i;
            }
            return keys.substr(i);
        }

        // One motion such as "w", "fx" or "gg"; text objects such as "iw" only after an operator or in visual mode.
        Token motion(std::string_view keys, bool textObject) {
            if (keys.empty())
                return Token::Incomplete;
            const char c      = keys[0];
            size_t     length = 1;
            if (has(charMotions, c) || c == 'g' || (textObject && (c == 'i' || c == 'a'))) {
                if (keys.size() < 2)
                    return Token::Incomplete;
                if ((c == 'g' && !has("gejkE_", keys[1])) || ((c == 'i' || c == 'a') && !has(textObjects, keys[1])))
                    return Token::Unknown;
                length = 2;
            } else if (!has(simpleMotions, c)) {
                return Token::Unknown;
            }
            return keys.size() == length ? Token::Normal : Token::Unknown;
        }

        // An operator with what it acts on: "cw", "c2w", "ciw", "cc", "gUiw". Only c goes into insert mode.
        Token operatorCommand(std::string_view op, std::string_view keys) {
            keys = withoutCount(keys);
            if (keys.empty())
                return Token::Incomplete;
            const Token done = keys == op.substr(op.size() - 1) ? Token::Normal : motion(keys, true);
            return done == Token::Normal && op == "c" ? Token::Insert : done;
        }

        // keys starts with g: "gi" goes into insert mode, "gU" is an operator, "gg" a motion.
        Token gCommand(std::string_view keys) {
            if (keys.size() < 2)
                return Token::Incomplete;
            if (has("uU~?", keys[1]))
                return operatorCommand(keys.substr(0, 2), keys.substr(2));
            if (keys.size() > 2)
                return Token::Unknown;
            if (keys[1] == 'i' || keys[1] == 'I')
                return Token::Insert;
            return has("gejkE_JpPv", keys[1]) ? Token::Normal : Token::Unknown;
        }

        Token normalCommand(std::string_view keys) {
            const char             c    = keys[0];
            const std::string_view rest = keys.substr(1);
            if (has("iaIAoOsSCR", c))
                return Token::Insert;
            if (has(":/?", c))
                return Token::CommandLine;
            if (has("vV", c))
                return Token::VisualStart;
            if (has("xXpPuJDY.~&*#", c))
                return Token::Normal;
            if (has("cdy<>=", c))
                return operatorCommand(keys.substr(0, 1), rest);
            if (c == 'Z')
                return rest.empty() ? Token::Incomplete : (has("ZQ", rest[0]) ? Token::Quit : Token::Normal);
            if (has("rmqz@", c))
                return rest.empty() ? Token::Incomplete : Token::Normal;
            if (c == 'g')
                return gCommand(keys);
            return motion(keys, false);
        }

        Token visualCommand(std::string_view keys) {
            const char c = keys[0];
            if (has("csSCRIA", c))
                return Token::Insert;
            if (c == ':')
                return Token::CommandLine;
            if (has("vdxXyYDJuU~<>=pP", c))
                return Token::VisualEnd;
            if (c == 'V' || c == 'o')
                return Token::Normal;
            return motion(keys, true);
        }

        // A register such as "a, then a count, then the command itself.
        Token command(std::string_view keys, bool visual) {
            if (!keys.empty() && keys[0] == '"') {
                if (keys.size() < 3)
                    return Token::Incomplete;
                keys.remove_prefix(2);
            }
            keys = withoutCount(keys);
            if (keys.empty())
                return Token::Incomplete;
            return visual ? visualCommand(keys) : normalCommand(keys);
        }

    } // namespace

    VimStep VimKeyReader::feed(char key) {
        pending_ += key;
        switch (command(pending_, visual_)) {
            case Token::Incomplete: return VimStep::Pending;
            case Token::Normal: break;
            case Token::VisualStart: visual_ = true; break;
            case Token::VisualEnd: visual_ = false; break;
            case Token::Insert: reset(); return VimStep::Insert;
            case Token::CommandLine: reset(); return VimStep::CommandLine;
            case Token::Unknown: reset(); return VimStep::Unknown;
            case Token::Quit: reset(); return VimStep::Quit;
        }
        pending_.clear();
        return VimStep::Pending;
    }

    void VimKeyReader::reset() {
        pending_.clear();
        visual_ = false;
    }

    namespace {

        std::string_view nextWord(std::string_view& line) {
            const size_t start = line.find_first_not_of(' ');
            if (start == std::string_view::npos) {
                line = {};
                return {};
            }
            line.remove_prefix(start);
            const size_t           end  = line.find(' ');
            const std::string_view word = line.substr(0, end);
            line.remove_prefix(end == std::string_view::npos ? line.size() : end);
            return word;
        }

    } // namespace

    bool startsVim(std::string_view shellLine) {
        constexpr std::array<std::string_view, 7> programs = {"vi", "vim", "nvim", "view", "vimdiff", "nvimdiff", "rvim"};
        std::string_view                          word     = nextWord(shellLine);
        // sudo and variables set for the command come before the program.
        while (word == "sudo" || word == "command" || word == "exec" || word.find('=') != std::string_view::npos)
            word = nextWord(shellLine);
        if (const size_t slash = word.rfind('/'); slash != std::string_view::npos)
            word.remove_prefix(slash + 1);
        return std::find(programs.begin(), programs.end(), word) != programs.end();
    }

    bool quitsVim(std::string_view commandLine) {
        constexpr std::array<std::string_view, 12> commands = {"q", "q!", "wq", "wq!", "x", "x!", "xa", "qa", "qa!", "wqa", "quit", "exit"};
        if (commandLine.empty() || commandLine[0] != ':')
            return false;
        commandLine.remove_prefix(1);
        const std::string_view command = nextWord(commandLine);
        return nextWord(commandLine).empty() && std::find(commands.begin(), commands.end(), command) != commands.end();
    }

} // namespace ngosen
