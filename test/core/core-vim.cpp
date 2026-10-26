/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Vim in a terminal (#302): keys typed in normal mode are commands that Vim takes for itself, and
// the word typed after them in insert mode must get its tone marks.
#include "fake-engine.h"
#include "fake-host.h"

#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-state.h"
#include "ngosen-utf8.h"
#include "ngosen-vim.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using ngosen::Field;
using ngosen::VimKeyReader;
using ngosen::VimStep;
using ngosen::test::FakeHost;
using ngosen::test::FakeKey;
using ngosen::test::FakeLoop;
using ngosen::test::FakeResources;

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what, const std::string& actual) {
        if (!ok) {
            std::cerr << "FAIL: " << what << "\n  got: " << actual << '\n';
            ++failures;
        }
    }

    // Plays Vim: keys in normal mode and on the command line never reach the text, so the input
    // method must not edit it then. In insert mode keys, commits and forwarded backspaces edit it.
    class FakeVim {
      public:
        explicit FakeVim(bool vimKeys) {
            realMode = ngosen::Mode::Sen;
            Field field;
            field.frontend = "wayland";
            auto owned     = std::make_unique<FakeHost>(loop_, field);
            host_          = owned.get();
            state_         = std::make_unique<ngosen::TypingState>(&resources_, std::move(owned));
            state_->setVimKeys(vimKeys);
        }

        void insert(const std::string& keys) {
            for (const char c : keys)
                press(static_cast<uint32_t>(c), 0, true);
        }
        void command(const std::string& keys) {
            for (const char c : keys)
                press(static_cast<uint32_t>(c), 0, false);
        }
        void escape() {
            press(ngosen::key::Escape, 0, false);
        }
        void ctrlBracket() {
            press(ngosen::key::bracketleft, ngosen::modifier::Ctrl, false);
        }
        void enter(bool inText) {
            press(ngosen::key::Return, 0, inText);
        }

        std::string result() const {
            return edited_.empty() ? text_ : text_ + " [edited in normal mode: " + edited_ + "]";
        }

      private:
        void press(uint32_t sym, uint32_t states, bool inText) {
            const size_t commits   = host_->commits().size();
            const size_t forwarded = host_->forwarded().size();
            FakeKey      down(sym, false, states);
            state_->keyEvent(down);
            FakeKey up(sym, true, states);
            state_->keyEvent(up);
            loop_.pump(60);
            if (!inText) {
                if (down.accepted() || host_->commits().size() > commits || host_->forwarded().size() > forwarded)
                    edited_ += static_cast<char>(sym & 0x7f);
                return;
            }
            if (!down.accepted())
                text_ += sym == ngosen::key::Return ? std::string("\n") : std::string(1, static_cast<char>(sym));
            for (size_t i = forwarded; i < host_->forwarded().size(); ++i) {
                if (host_->forwarded()[i].key == ngosen::EditKey::BackSpace && !host_->forwarded()[i].release)
                    ngosen::utf8::eraseLastCodepoint(text_);
            }
            for (size_t i = commits; i < host_->commits().size(); ++i)
                text_ += host_->commits()[i];
        }

        FakeLoop                             loop_;
        FakeResources                        resources_;
        FakeHost*                            host_ = nullptr;
        std::unique_ptr<ngosen::TypingState> state_;
        std::string                          text_;
        std::string                          edited_;
    };

    VimStep lastStep(const std::string& keys) {
        VimKeyReader reader;
        VimStep      step = VimStep::Pending;
        for (const char c : keys)
            step = reader.feed(c);
        return step;
    }

    void testReader() {
        for (const char* keys : {"i",    "a",   "A",   "o",     "O",   "s",  "S",      "C",   "cc",  "cw",    "c2w",
                                 "ciw",  "ci(", "caw", "c$",    "ctx", "gi", "3wcw",   "dwi", "ddO", "ggdGi", "\"ayyp\"bP0i",
                                 "viwc", "Vjs", "fxa", "yiwPa", "xi",  "ea", "jjkciw", "mai", "u.A", "gUiwA"}) {
            check(lastStep(keys) == VimStep::Insert, std::string("reader: ") + keys + " goes into insert mode", std::to_string(static_cast<int>(lastStep(keys))));
        }
        for (const char* keys : {"c", "ci", "d", "dd", "jj", "3w", "viw", "gg", "fx", "dap", "yy", "\"a", "\"ay", "gU", "x"}) {
            check(lastStep(keys) == VimStep::Pending, std::string("reader: ") + keys + " stays in normal mode", std::to_string(static_cast<int>(lastStep(keys))));
        }
        for (const char* keys : {":", "/", "?", "3:"})
            check(lastStep(keys) == VimStep::CommandLine, std::string("reader: ") + keys + " opens the command line", std::to_string(static_cast<int>(lastStep(keys))));
        for (const char* keys : {"!", "gs", "c!", "K"})
            check(lastStep(keys) == VimStep::Unknown, std::string("reader: ") + keys + " is not a command it knows", std::to_string(static_cast<int>(lastStep(keys))));
    }

    void expect(const std::string& what, const FakeVim& vim, const std::string& want) {
        check(vim.result() == want, what, vim.result());
    }

    void testInsertCommands() {
        for (const char* keys : {"i", "a", "A", "o", "s", "S", "cc", "cw", "ciw", "ea", "xi", "jjkciw", "3wcw", "ggdGi", "viwc"}) {
            FakeVim vim(true);
            vim.insert("xin chao ");
            vim.escape();
            vim.command(keys);
            vim.insert("tieengs vieetj");
            expect(std::string("Escape, ") + keys + ", tieengs vieetj", vim, "xin chao tiếng việt");
        }
    }

    // The command letter fuses with the word's first letter: "a" then "an" is not "aan", which would be "ân".
    void testCommandLetterStartsTheWord() {
        const std::vector<std::pair<const char*, const char*>> cases = {{"a", "an "}, {"o", "oong "}, {"a", "aay "}, {"i", "ichs "}, {"s", "sangs "}};
        const std::vector<const char*>                         want  = {"an ", "ông ", "ây ", "ích ", "sáng "};
        for (size_t i = 0; i < cases.size(); ++i) {
            FakeVim vim(true);
            vim.escape();
            vim.command(cases[i].first);
            vim.insert(cases[i].second);
            expect(std::string("Escape, ") + cases[i].first + ", " + cases[i].second, vim, want[i]);
        }
    }

    void testOtherWaysOut() {
        {
            FakeVim vim(true);
            vim.ctrlBracket();
            vim.command("ciw");
            vim.insert("tieengs");
            expect("Ctrl+[, ciw, tieengs", vim, "tiếng");
        }
        // The command line takes text until Enter, then Vim is back in normal mode.
        {
            FakeVim vim(true);
            vim.escape();
            vim.command(":");
            vim.command("w");
            vim.enter(false);
            vim.command("o");
            vim.insert("tieengs");
            expect("Escape, :w, Enter, o, tieengs", vim, "tiếng");
        }
        {
            FakeVim vim(true);
            vim.escape();
            vim.command("dd");
            vim.enter(false);
            vim.command("i");
            vim.insert("vieetj");
            expect("Escape, dd, Enter, i, vieetj", vim, "việt");
        }
    }

    // Keys that are no Vim command were text after all and form a word with the keys after them.
    void testNotVim() {
        FakeVim vim(true);
        vim.escape();
        vim.insert("tieengs vieetj");
        expect("Vim keys on, Escape in an app without Vim, tieengs vieetj", vim, "tiếng việt");
    }

    void testVimKeysOff() {
        FakeVim vim(false);
        vim.escape();
        vim.insert("anhs em");
        expect("Vim keys off, Escape, anhs em", vim, "ánh em");
    }

} // namespace

int main() {
    testReader();
    testInsertCommands();
    testCommandLetterStartsTheWord();
    testOtherWaysOut();
    testNotVim();
    testVimKeysOff();
    if (failures == 0)
        std::cout << "core vim: all passed\n";
    return failures == 0 ? 0 : 1;
}
