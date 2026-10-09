// SPDX-License-Identifier: GPL-3.0-or-later
//
// LibreOffice Calc over the fcitx Qt module sends nothing when the user clicks another cell. It drops
// the surrounding text capability before each key and reports the field right after it, and a
// backspace on a cell that is not being edited opens the Delete Contents dialog.
#include "lotus-engine.h"
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
        FakeCalc(TestInstance& testInstance, fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry) :
            testInstance_(testInstance), engine_(engine), entry_(entry), context_(std::make_unique<TestInputContext>(&testInstance.instance, "soffice.bin", "dbus")) {
            context_->setCapabilityFlags(withSurrounding);
            context_->focusIn();
            fcitx::InputContextEvent focus(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, focus);
        }

        void type(const std::string& keys) {
            for (char c : keys) {
                const size_t forwarded = context_->forwarded().size();
                const size_t commits   = context_->commits().size();
                context_->setCapabilityFlags(keyOrderFix);
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
        fcitx::LotusEngine&               engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
        std::string                       cells_;
        bool                              editing_      = false;
        bool                              dialogOpened_ = false;
    };
} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-cell-click-key-report");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");

    FakeCalc                calc(testInstance, engine, entry);
    calc.type("dduwowcj");
    calc.clickCell();
    calc.type("tieengs");
    calc.clickCell();
    calc.type("chaof");
    const std::string expected = "được|tiếng|chào";
    if (calc.dialogOpened() || calc.cells() != expected) {
        std::cerr << "Expected: " << expected << "\nActual: " << calc.cells() << (calc.dialogOpened() ? " (Delete Contents opened)" : "") << '\n';
        return 1;
    }
    return 0;
}
