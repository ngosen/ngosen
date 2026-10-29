// SPDX-License-Identifier: GPL-3.0-or-later
//
// LibreOffice Calc over the fcitx Qt module sends nothing when the user clicks another cell. It drops
// the surrounding text capability before each key and reports the field right after it, and a
// backspace on a cell that is not being edited opens the Delete Contents dialog.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
    const fcitx::CapabilityFlags keyOrderFix{fcitx::CapabilityFlag::KeyEventOrderFix};
    const fcitx::CapabilityFlags withSurrounding = keyOrderFix | fcitx::CapabilityFlag::SurroundingText;

    class FakeCalc {
      public:
        // keepsFlagOnKey: the field keeps the surrounding text capability during a key, as Writer
        // under the program name LibreOffice reports over the GTK module does.
        FakeCalc(TestInstance& testInstance, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, bool keepsFlagOnKey = false) :
            testInstance_(testInstance), engine_(engine), entry_(entry),
            context_(std::make_unique<TestInputContext>(&testInstance.instance, keepsFlagOnKey ? "soffice" : "soffice.bin", "dbus")), keepsFlagOnKey_(keepsFlagOnKey) {
            context_->setCapabilityFlags(withSurrounding);
            context_->focusIn();
            fcitx::InputContextEvent focus(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, focus);
        }

        void type(const std::string& keys) {
            for (char c : keys) {
                const size_t forwarded = context_->forwarded().size();
                const size_t commits   = context_->commits().size();
                const size_t deletes   = context_->deletes().size();
                context_->setCapabilityFlags(keepsFlagOnKey_ ? withSurrounding : keyOrderFix);
                fcitx::KeyEvent event(context_.get(), fcitx::Key(static_cast<fcitx::KeySym>(c)), false);
                engine_.keyEvent(entry_, event);
                context_->setCapabilityFlags(withSurrounding);
                report();
                if (!event.accepted())
                    insert(std::string(1, c));
                pumpEventLoop(testInstance_.instance, 60);
                for (size_t i = forwarded; i < context_->forwarded().size(); ++i) {
                    const auto& key = context_->forwarded()[i];
                    if (key.key().sym() != FcitxKey_BackSpace || key.isRelease())
                        continue;
                    if (!editing_)
                        dialogOpened_ = true;
                    else if (!text_.empty())
                        eraseLastChar();
                    ++backspaces_;
                }
                for (size_t i = deletes; i < context_->deletes().size(); ++i) {
                    // Only deletions that end at the cursor, at the end of the cell, are sent here.
                    for (unsigned int n = 0; n < context_->deletes()[i].second && !text_.empty(); ++n)
                        eraseLastChar();
                    ++surroundingDeletes_;
                }
                for (size_t i = commits; i < context_->commits().size(); ++i)
                    insert(context_->commits()[i]);
                report();
                pumpEventLoop(testInstance_.instance, 5);
            }
        }

        // Selecting another cell ends editing; Calc tells the IM nothing.
        void clickCell() {
            cells_ += text_ + "|";
            text_.clear();
            editing_ = false;
        }

        std::string cells() const {
            return cells_ + text_;
        }
        bool dialogOpened() const {
            return dialogOpened_;
        }
        int backspaces() const {
            return backspaces_;
        }
        int surroundingDeletes() const {
            return surroundingDeletes_;
        }

      private:
        void insert(const std::string& s) {
            text_ += s;
            editing_ = true;
        }
        void eraseLastChar() {
            size_t start = text_.size();
            while (start > 0 && (static_cast<unsigned char>(text_[--start]) & 0xC0) == 0x80) {}
            text_.erase(start);
        }
        void report() {
            if (editing_) {
                context_->surroundingText().setText(text_, fcitx::utf8::length(text_), fcitx::utf8::length(text_));
            } else {
                context_->surroundingText().invalidate();
            }
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 1);
        }

        TestInstance&                     testInstance_;
        fcitx::NgoSenEngine&              engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
        std::string                       cells_;
        bool                              keepsFlagOnKey_     = false;
        bool                              editing_            = false;
        bool                              dialogOpened_       = false;
        int                               backspaces_         = 0;
        int                               surroundingDeletes_ = 0;
    };
} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-cell-click-key-report");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");

    int                     failures = 0;
    for (const bool keepsFlagOnKey : {false, true}) {
        FakeCalc calc(testInstance, engine, entry, keepsFlagOnKey);
        // Writer has no cells to click; its words go into one paragraph.
        if (keepsFlagOnKey) {
            calc.type("dduwowcj tieengs chaof");
        } else {
            calc.type("dduwowcj");
            calc.clickCell();
            calc.type("tieengs");
            calc.clickCell();
            calc.type("chaof");
        }
        const std::string expected = keepsFlagOnKey ? "được tiếng chào" : "được|tiếng|chào";
        // A field that keeps the flag deletes through its surrounding text, never with a backspace.
        const bool pathOk = !keepsFlagOnKey || (calc.surroundingDeletes() > 0 && calc.backspaces() == 0);
        if (calc.dialogOpened() || calc.cells() != expected || !pathOk) {
            std::cerr << (keepsFlagOnKey ? "Writer-like: " : "Calc: ") << "Expected: " << expected << "\nActual: " << calc.cells()
                      << (calc.dialogOpened() ? " (Delete Contents opened)" : "") << " deletions=" << calc.surroundingDeletes() << " backspaces=" << calc.backspaces() << '\n';
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
