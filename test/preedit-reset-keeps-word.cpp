// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dolphin's search field rewrites its text while the user types (#717). On the Wayland text-input
// protocol Qt drops the underlined word on that reset, so the input method has to keep and redraw it.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <iostream>
#include <memory>
#include <string>

namespace {

    // Plays the app: applies commits and keys to its text, shows the preedit at the cursor and reports the text.
    class PreeditApp {
      public:
        PreeditApp(TestInstance& testInstance, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, const std::string& frontend) :
            testInstance_(testInstance), engine_(engine), entry_(entry), context_(std::make_unique<TestInputContext>(&testInstance.instance, "org.kde.dolphin", frontend)) {
            context_->setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText});
            context_->focusIn();
            fcitx::InputContextEvent focus(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, focus);
            report();
        }

        void type(const std::string& keys) {
            for (const char c : keys)
                press(c == ' ' ? FcitxKey_space : static_cast<fcitx::KeySym>(c), std::string(1, c));
        }

        void backspace() {
            press(FcitxKey_BackSpace, "");
        }

        // Qt drops the preedit on a reset; fcitx5's own Qt and GTK modules commit it first. GTK, Firefox and
        // Chromium over Wayland commit it after the reset, Chromium reporting the old text once more first.
        enum class OnReset {
            Drop,
            CommitFirst,
            CommitAfter,
            CommitAfterOldReport
        };

        void reset(OnReset how = OnReset::Drop) {
            const std::string preedit = preedit_;
            if (how == OnReset::CommitFirst) {
                insert(preedit);
                report();
            }
            preedit_.clear();
            fcitx::InputContextEvent event(context_.get(), fcitx::EventType::InputContextReset);
            engine_.reset(entry_, event);
            readPreedit();
            if (how == OnReset::CommitAfterOldReport)
                report();
            if (how == OnReset::CommitAfter || how == OnReset::CommitAfterOldReport) {
                insert(preedit);
                report();
                readPreedit();
            }
        }

        void moveToStart() {
            cursor_ = 0;
            report();
        }

        void clear() {
            text_.clear();
            cursor_ = 0;
            report();
        }

        void selectAll() {
            context_->surroundingText().setText(text_, fcitx::utf8::length(text_), 0);
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 5);
        }

        void invalidateReport() {
            context_->surroundingText().invalidate();
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 5);
        }

        std::string shown() const {
            return text_.substr(0, cursor_) + preedit_ + text_.substr(cursor_);
        }

      private:
        void press(fcitx::KeySym sym, const std::string& text) {
            const size_t    commits = context_->commits().size();
            const size_t    deletes = context_->deletes().size();
            fcitx::KeyEvent down(context_.get(), fcitx::Key(sym), false);
            engine_.keyEvent(entry_, down);
            pumpEventLoop(testInstance_.instance, 5);
            for (size_t i = deletes; i < context_->deletes().size(); ++i) {
                const auto [offset, size] = context_->deletes()[i];
                for (unsigned int n = 0; offset == -static_cast<int>(size) && n < size; ++n)
                    eraseBeforeCursor();
            }
            for (size_t i = commits; i < context_->commits().size(); ++i)
                insert(context_->commits()[i]);
            if (!down.accepted()) {
                if (sym == FcitxKey_BackSpace)
                    eraseBeforeCursor();
                else
                    insert(text);
            }
            fcitx::KeyEvent up(context_.get(), fcitx::Key(sym), true);
            engine_.keyEvent(entry_, up);
            readPreedit();
            report();
        }

        void readPreedit() {
            preedit_ = context_->inputPanel().clientPreedit().toString();
        }

        void eraseBeforeCursor() {
            if (cursor_ == 0)
                return;
            size_t start = cursor_;
            while (start > 0 && (static_cast<unsigned char>(text_[--start]) & 0xC0) == 0x80) {}
            text_.erase(start, cursor_ - start);
            cursor_ = start;
        }

        void insert(const std::string& s) {
            text_.insert(cursor_, s);
            cursor_ += s.size();
        }

        void report() {
            const auto cursor = fcitx::utf8::length(text_.substr(0, cursor_));
            context_->surroundingText().setText(text_, cursor, cursor);
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 5);
        }

        TestInstance&                     testInstance_;
        fcitx::NgoSenEngine&              engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
        std::string                       preedit_;
        size_t                            cursor_ = 0;
    };

    bool expect(const std::string& name, const PreeditApp& app, const std::string& expected) {
        if (app.shown() == expected)
            return true;
        std::cerr << "FAIL: " << name << "\n  expected: '" << expected << "'\n  got:      '" << app.shown() << "'\n";
        return false;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-preedit-reset-keeps-word");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    bool                    ok = true;

    // Dolphin resets twice, about 500 ms after each edit.
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi l");
        app.reset();
        app.reset();
        app.type("af ai ");
        ok &= expect("a reset keeps the word being typed", app, "tôi là ai ");
    }

    // Backspace takes the last word back into the preedit; a reset then must not drop it.
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi laf ");
        app.backspace();
        app.reset();
        app.type(" ");
        ok &= expect("a reset keeps a word taken back with Backspace", app, "tôi là ");
    }

    // fcitx5's own Qt module commits the preedit on a reset, so keeping the word would type it twice.
    {
        PreeditApp app(testInstance, engine, entry, "dbus");
        app.type("tooi l");
        app.reset(PreeditApp::OnReset::CommitFirst);
        app.type("af ");
        ok &= expect("an app that commits the preedit on a reset does not get it twice", app, "tôi là ");
    }

    // A click in GTK or Firefox: the app commits the word after the reset.
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi l");
        app.reset(PreeditApp::OnReset::CommitAfter);
        app.type("af ");
        ok &= expect("an app that commits the word after the reset does not get it twice", app, "tôi là ");
    }

    // A click in Chromium: the old text once more, then the committed word with the cursor at the click.
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi bawn");
        app.reset(PreeditApp::OnReset::CommitAfterOldReport);
        app.moveToStart();
        app.type("k ");
        ok &= expect("a click that commits the word does not move it to the click", app, "k tôi băn");
    }

    // Without a usable report the field may have changed under the word.
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi l");
        app.invalidateReport();
        app.reset();
        app.type("af ");
        ok &= expect("a reset with no valid report drops the word", app, "tôi à ");
    }
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi l");
        app.selectAll();
        app.reset();
        ok &= expect("a reset with a selection drops the word", app, "tôi ");
    }

    // The app clears the field under the word: the word is still on screen and goes on there.
    {
        PreeditApp app(testInstance, engine, entry, "wayland");
        app.type("tooi l");
        app.clear();
        app.reset();
        app.type("af ");
        ok &= expect("a field cleared under the word keeps only that word", app, "là ");
    }

    return ok ? 0 : 1;
}
