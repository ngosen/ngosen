// SPDX-License-Identifier: GPL-3.0-or-later
//
// On Wayland the IM sees no mouse click; the app only reports the cursor somewhere else in the same text.
#include "lotus-engine.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
    const std::string zeroWidthTail = "\u200b\u200b\u200b\u200b";

    void              reportFailure(const std::string& step, const std::string& expected, const std::string& actual) {
        std::cerr << "Step: " << step << "\nExpected: " << expected << "\nActual: " << actual << '\n';
    }

    // Plays the app: applies keys, forwarded backspaces and commits to its text and reports it back.
    class FakeApp {
      public:
        FakeApp(TestInstance& testInstance, fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry) :
            testInstance_(testInstance), engine_(engine), entry_(entry), context_(std::make_unique<TestInputContext>(&testInstance.instance, "gtk4app", "wayland")) {
            context_->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
            context_->focusIn();
            fcitx::InputContextEvent focus(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, focus);
            report();
        }

        void type(const std::string& keys) {
            for (char c : keys) {
                const size_t    forwarded = context_->forwarded().size();
                const size_t    commits   = context_->commits().size();
                fcitx::KeyEvent event(context_.get(), fcitx::Key(static_cast<fcitx::KeySym>(c)), false);
                engine_.keyEvent(entry_, event);
                if (!event.accepted())
                    insert(std::string(1, c));
                for (size_t i = forwarded; i < context_->forwarded().size(); ++i) {
                    const auto& key = context_->forwarded()[i];
                    if (key.key().sym() == FcitxKey_BackSpace && !key.isRelease())
                        eraseBeforeCursor();
                }
                report();
                pumpEventLoop(testInstance_.instance, 60);
                if (context_->commits().size() > commits) {
                    for (size_t i = commits; i < context_->commits().size(); ++i) {
                        if (cursorBeforeText_) {
                            report();
                            report();
                            report(fcitx::utf8::length(context_->commits()[i]));
                        }
                        insert(context_->commits()[i]);
                    }
                    report();
                }
            }
        }

        void click(size_t cursor) {
            cursor_ = cursor;
            report();
        }

        void repeatReport() {
            report();
        }

        // Firefox in some web editors repeats the old state, then reports the cursor after a commit
        // before the committed text.
        // Those editors keep zero-width spaces after the caret, so the early cursor is still in range.
        void reportCursorBeforeText() {
            cursorBeforeText_ = true;
            text_             = zeroWidthTail;
            report();
        }

        const std::string& text() const {
            return text_;
        }

      private:
        void eraseBeforeCursor() {
            size_t start = cursor_;
            while (start > 0 && (static_cast<unsigned char>(text_[--start]) & 0xC0) == 0x80) {}
            text_.erase(start, cursor_ - start);
            cursor_ = start;
        }
        void insert(const std::string& s) {
            text_.insert(cursor_, s);
            cursor_ += s.size();
        }
        // advance moves the reported cursor ahead of the text the app has applied so far.
        void report(size_t advance = 0) {
            const auto cursor = fcitx::utf8::length(text_.substr(0, cursor_)) + advance;
            context_->surroundingText().setText(text_, cursor, cursor);
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 5);
        }

        TestInstance&                     testInstance_;
        fcitx::LotusEngine&               engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
        size_t                            cursor_           = 0;
        bool                              cursorBeforeText_ = false;
    };

    bool expectText(const std::string& step, const FakeApp& app, const std::string& expected) {
        if (app.text() == expected)
            return true;
        reportFailure(step, "\"" + expected + "\"", "\"" + app.text() + "\"");
        return false;
    }
} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-cursor-jump-reset");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    engine.setConfig(config);
    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");

    // Apps repeat an unchanged report; that is not a click and must keep the word.
    {
        FakeApp app(testInstance, engine, entry);
        app.type("tie");
        app.repeatReport();
        app.type("e");
        if (!expectText("type \"tiee\" with a repeated report in between", app, "tiê"))
            return 1;
    }
    // A click at the start of the field must start a new word there.
    {
        FakeApp app(testInstance, engine, entry);
        app.type("tieng");
        app.click(0);
        app.type("as");
        if (!expectText("type \"tieng\", click at the start, type \"as\"", app, "átieng"))
            return 1;
    }
    // The early cursor report is the echo of our own commit, not a click.
    {
        FakeApp app(testInstance, engine, entry);
        app.reportCursorBeforeText();
        app.type("veef naaus");
        if (!expectText("type \"veef naaus\" with the cursor reported before the text", app, "về nấu" + zeroWidthTail))
            return 1;
    }
    return 0;
}
