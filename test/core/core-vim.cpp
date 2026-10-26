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
using ngosen::quitsVim;
using ngosen::startsVim;
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
        // reported is the text a field that declares surrounding text sends; null for none declared.
        explicit FakeVim(bool vimKeys, ngosen::Mode mode = ngosen::Mode::Sen, const char* reported = nullptr) {
            realMode = mode;
            Field field;
            field.frontend        = "wayland";
            field.preedit         = mode == ngosen::Mode::Preedit;
            field.surroundingText = reported != nullptr;
            preedit_              = field.preedit;
            auto owned            = std::make_unique<FakeHost>(loop_, field);
            host_                 = owned.get();
            if (reported != nullptr)
                report(reported);
            state_ = std::make_unique<ngosen::TypingState>(&resources_, std::move(owned));
            state_->setVimKeys(vimKeys);
        }

        void insert(const std::string& keys) {
            for (const char c : keys)
                press(static_cast<uint32_t>(c), 0, &text_);
        }
        void command(const std::string& keys) {
            for (const char c : keys)
                press(static_cast<uint32_t>(c), 0, nullptr);
        }
        // Vim's command line takes text of its own, which never reaches the file.
        void commandLine(const std::string& keys) {
            for (const char c : keys)
                press(static_cast<uint32_t>(c), 0, &line_);
        }
        void escape() {
            press(ngosen::key::Escape, 0, nullptr);
        }
        void ctrlBracket() {
            press(ngosen::key::bracketleft, ngosen::modifier::Ctrl, nullptr);
        }
        void ctrlC() {
            press(ngosen::key::c, ngosen::modifier::Ctrl, nullptr);
        }
        void left() {
            press(ngosen::key::Left, 0, nullptr);
        }
        void backspace() {
            press(ngosen::key::BackSpace, 0, nullptr);
        }
        void enter(bool inText) {
            press(ngosen::key::Return, 0, inText ? &text_ : nullptr);
        }
        void runCommandLine() {
            press(ngosen::key::Return, 0, &line_);
            lastLine_ = line_;
            line_.clear();
        }
        const std::string& lastCommandLine() const {
            return lastLine_;
        }
        void report(const std::string& text) {
            host_->setSurrounding(text, static_cast<unsigned int>(ngosen::utf8::length(text)));
        }
        ngosen::Options& options() {
            return resources_.mutableOptions();
        }

        std::string result() const {
            return edited_.empty() ? text_ : text_ + " [edited in normal mode: " + edited_ + "]";
        }

      private:
        // target is where the app puts what is typed; null for normal mode, where nothing may be typed.
        void press(uint32_t sym, uint32_t states, std::string* target) {
            const size_t commits   = host_->commits().size();
            const size_t forwarded = host_->forwarded().size();
            FakeKey      down(sym, false, states);
            state_->keyEvent(down);
            FakeKey up(sym, true, states);
            state_->keyEvent(up);
            // Typed text waits out the forward wait, Enter the held Enter; a commit arriving during a command still counts as an edit.
            loop_.pump(target != &text_ ? 5 : sym == ngosen::key::Return ? 60 : 20);
            if (target == nullptr) {
                // A key the app gets changed, such as j turned into J, is a different command.
                if (down.accepted() || down.name() != std::to_string(sym) || host_->commits().size() > commits || host_->forwarded().size() > forwarded)
                    edited_ += static_cast<char>(sym & 0x7f);
                return;
            }
            const std::string passed = down.accepted() ? std::string() : sym == ngosen::key::Return ? std::string("\n") : std::string(1, static_cast<char>(sym));
            // Sen lets the key through and then rewrites the word; Preedit commits the word before the key that ends it.
            if (!preedit_)
                *target += passed;
            for (size_t i = forwarded; i < host_->forwarded().size(); ++i) {
                if (host_->forwarded()[i].key == ngosen::EditKey::BackSpace && !host_->forwarded()[i].release)
                    ngosen::utf8::eraseLastCodepoint(*target);
            }
            for (size_t i = commits; i < host_->commits().size(); ++i)
                *target += host_->commits()[i];
            if (preedit_)
                *target += passed;
        }

        FakeLoop                             loop_;
        FakeResources                        resources_;
        FakeHost*                            host_ = nullptr;
        std::unique_ptr<ngosen::TypingState> state_;
        std::string                          text_;
        std::string                          line_;
        std::string                          lastLine_;
        bool                                 preedit_ = false;
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
        for (const char* keys : {"ZZ", "ZQ", "jjZZ"})
            check(lastStep(keys) == VimStep::Quit, std::string("reader: ") + keys + " exits Vim", std::to_string(static_cast<int>(lastStep(keys))));
        for (const char* line : {"vim", "nvim notes.md", "  vim a b", "sudo vim /etc/hosts", "/usr/bin/nvim", "TERM=xterm nvim", "vimdiff a b"})
            check(startsVim(line), std::string("\"") + line + "\" starts Vim", "false");
        for (const char* line : {"", "vimtutor", "echo vim", "git commit", "sudo", "nvimx", "vi phamj"})
            check(!startsVim(line), std::string("\"") + line + "\" does not start Vim", "true");
        for (const char* line : {":q", ":q!", ":wq", ":x", ":qa!", ": wq ", ":wqa!", ":xit", ":cq"})
            check(quitsVim(line), std::string("\"") + line + "\" exits Vim", "false");
        for (const char* line : {":w", "/q", "q", ":q foo", ":", ":wqx"})
            check(!quitsVim(line), std::string("\"") + line + "\" does not exit Vim", "true");
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

    // Backspace on the bare ":" closes the command line; a search for "q" is no :q.
    void testCommandLine() {
        {
            FakeVim vim(true);
            vim.escape();
            vim.commandLine(":");
            vim.backspace();
            vim.command("i");
            vim.insert("tieengs");
            expect("Escape, :, Backspace, i, tieengs", vim, "tiếng");
        }
        {
            FakeVim vim(true);
            vim.escape();
            vim.commandLine("/q");
            vim.runCommandLine();
            vim.command("a");
            vim.insert("an ");
            expect("Escape, /q, Enter, a, an", vim, "an ");
        }
    }

    // An ex command reaches Vim as typed, so Telex does not turn ":s/aa/b" into ":s/â/b".
    void testExCommandAsTyped() {
        FakeVim vim(true);
        vim.escape();
        vim.commandLine(":s/aa/b");
        vim.runCommandLine();
        check(vim.lastCommandLine() == ":s/aa/b\n", "Escape, :s/aa/b, Enter reaches Vim as typed", vim.lastCommandLine());
    }

    // Auto-capitalize would turn the command j into J, which joins lines.
    void testNoCapitalInNormalMode() {
        FakeVim vim(true);
        vim.options().autoCapitalizeAfterPunctuation = true;
        vim.escape();
        vim.enter(false);
        vim.command("jciw");
        vim.insert("tieengs");
        expect("auto-capitalize, Escape, Enter, jciw, tieengs", vim, "tiếng");
    }

    // c then an arrow key is a whole command in Vim, so the letters after it start anew.
    void testArrowEndsPendingCommand() {
        FakeVim vim(true);
        vim.escape();
        vim.command("c");
        vim.left();
        vim.insert("tieengs");
        expect("Escape, c, Left, tieengs", vim, "tiếng");
    }

    // Preedit holds the letters it types; in normal mode it must hold none.
    void testPreedit() {
        for (const char* keys : {"i", "a", "ciw", "ddO", "jjkciw", "3wcw"}) {
            FakeVim vim(true, ngosen::Mode::Preedit);
            vim.escape();
            vim.command(keys);
            vim.insert("tieengs vieetj ");
            expect(std::string("Preedit, Escape, ") + keys + ", tieengs vieetj", vim, "tiếng việt ");
        }
        {
            FakeVim vim(true, ngosen::Mode::Preedit);
            vim.insert("xin ");
            vim.escape();
            vim.command("dd");
            vim.enter(false);
            vim.command("o");
            vim.insert("chaof ");
            expect("Preedit, Escape, dd, Enter, o, chaof", vim, "xin chào ");
        }
    }

    // Vim started from the shell opens in normal mode, without the Vim mode switch.
    void testStartedFromShell() {
        for (const char* line : {"nvim", "vim", "sudo vim"}) {
            FakeVim vim(false);
            vim.insert(line);
            vim.enter(true);
            vim.command("ciw");
            vim.insert("tieengs");
            expect(std::string(line) + ", Enter, ciw, tieengs", vim, std::string(line) + "\ntiếng");
        }
        {
            FakeVim vim(false, ngosen::Mode::Preedit);
            vim.insert("nvim ");
            vim.enter(true);
            vim.command("a");
            vim.insert("an ");
            expect("Preedit, nvim, Enter, a, an", vim, "nvim \nan ");
        }
        for (const char* line : {"echo vim", "vi phamj"}) {
            FakeVim vim(false);
            vim.insert(line);
            vim.enter(true);
            vim.insert("anhs ");
            const std::string shown = std::string(line) == "vi phamj" ? "vi phạm" : line;
            expect(std::string(line) + ", Enter, anhs", vim, shown + "\nánh ");
        }
        // Typed while Vim runs, the word vim on a line of its own does not start it again.
        {
            FakeVim vim(false);
            vim.insert("nvim");
            vim.enter(true);
            vim.command("o");
            vim.insert("vim");
            vim.enter(true);
            vim.insert("anhs ");
            expect("nvim, o, vim, Enter, anhs", vim, "nvim\nvim\nánh ");
        }
    }

    // After Vim exits, the shell takes text again from its first key.
    // kitty declares surrounding text but always sends it empty, like a field that reports nothing.
    void testFieldWithEmptyReport() {
        {
            FakeVim vim(false, ngosen::Mode::Sen, "");
            vim.insert("nvim");
            vim.enter(true);
            vim.command("ciw");
            vim.insert("tieengs");
            expect("empty report: nvim, Enter, ciw, tieengs", vim, "nvim\ntiếng");
        }
        {
            FakeVim vim(true, ngosen::Mode::Sen, "");
            vim.insert("xin chao ");
            vim.escape();
            vim.command("ciw");
            vim.insert("tieengs vieetj");
            expect("empty report, Vim mode on: Escape, ciw, tieengs vieetj", vim, "xin chao tiếng việt");
        }
        // A field that holds its text is no shell, whatever line is typed into it.
        {
            FakeVim vim(false, ngosen::Mode::Sen, "");
            vim.insert("vim");
            vim.report("vim");
            vim.enter(true);
            vim.insert("oo ");
            expect("reported text: vim, Enter, oo", vim, "vim\nô ");
        }
    }

    void testQuit() {
        for (const char* quit : {":wq", ":q!"}) {
            FakeVim vim(false);
            vim.insert("nvim");
            vim.enter(true);
            vim.commandLine(quit);
            vim.runCommandLine();
            vim.insert("anhs ");
            expect(std::string("nvim, ") + quit + ", anhs", vim, "nvim\nánh ");
        }
        {
            FakeVim vim(false);
            vim.insert("nvim");
            vim.enter(true);
            vim.command("ZZ");
            vim.insert("anhs ");
            expect("nvim, ZZ, anhs", vim, "nvim\nánh ");
        }
        {
            FakeVim vim(false);
            vim.insert("nvim");
            vim.enter(true);
            vim.command("ZZ");
            vim.insert("vim");
            vim.enter(true);
            vim.command("ciw");
            vim.insert("tieengs");
            expect("nvim, ZZ, vim, Enter, ciw, tieengs", vim, "nvim\nvim\ntiếng");
        }
        // Ctrl+C also leaves insert mode once Vim is known to run.
        {
            FakeVim vim(false);
            vim.insert("nvim");
            vim.enter(true);
            vim.command("i");
            vim.insert("xin ");
            vim.ctrlC();
            vim.command("ciw");
            vim.insert("tieengs");
            expect("nvim, i, xin, Ctrl+C, ciw, tieengs", vim, "nvim\nxin tiếng");
        }
        {
            FakeVim vim(false);
            vim.insert("nvim");
            vim.enter(true);
            vim.commandLine(":w");
            vim.runCommandLine();
            vim.command("a");
            vim.insert("an ");
            expect("nvim, :w, a, an", vim, "nvim\nan ");
        }
    }

} // namespace

int main() {
    testReader();
    testInsertCommands();
    testCommandLetterStartsTheWord();
    testOtherWaysOut();
    testNotVim();
    testVimKeysOff();
    testCommandLine();
    testExCommandAsTyped();
    testNoCapitalInNormalMode();
    testArrowEndsPendingCommand();
    testPreedit();
    testStartedFromShell();
    testQuit();
    testFieldWithEmptyReport();
    if (failures == 0)
        std::cout << "core vim: all passed\n";
    return failures == 0 ? 0 : 1;
}
