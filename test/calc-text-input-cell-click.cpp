// SPDX-License-Identifier: GPL-3.0-or-later
//
// LibreOffice Calc over Wayland text-input sends nothing when the user clicks another cell. It reports
// the field only when a key reaches it: first the state the key found, then the state after it. Our
// commits and deletions show only in its next report, and a backspace on a cell that is not being
// edited opens the Delete Contents dialog.
#include "lotus-engine.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <iostream>
#include <memory>
#include <string>

namespace {
    const fcitx::CapabilityFlags withSurrounding = fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText} | fcitx::CapabilityFlag::Preedit;

    class FakeCalc {
      public:
        FakeCalc(TestInstance& testInstance, fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry) :
            testInstance_(testInstance), engine_(engine), entry_(entry), context_(std::make_unique<TestInputContext>(&testInstance.instance, "libreoffice-calc", "wayland")) {
            context_->setCapabilityFlags(withSurrounding);
            context_->focusIn();
            fcitx::InputContextEvent focus(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, focus);
        }

        void type(const std::string& keys) {
            for (char c : keys) {
                fcitx::KeyEvent event(context_.get(), fcitx::Key(static_cast<fcitx::KeySym>(c)), false);
                engine_.keyEvent(entry_, event);
                if (!event.accepted())
                    keyArrives(std::string(1, c));
                for (int step = 0; step < 20; ++step) {
                    pumpEventLoop(testInstance_.instance, 3);
                    applyEdits();
                }
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
        // A typed key starts editing a selected cell, replacing what it held.
        void keyArrives(const std::string& text) {
            report();
            if (!editing_)
                text_.clear();
            editing_ = true;
            text_ += text;
            report();
        }
        void applyEdits() {
            for (; forwardedSeen_ < context_->forwarded().size(); ++forwardedSeen_) {
                const auto& key = context_->forwarded()[forwardedSeen_];
                if (key.isRelease())
                    continue;
                if (key.key().sym() == FcitxKey_BackSpace) {
                    if (!editing_)
                        dialogOpened_ = true;
                    else
                        eraseChars(1);
                } else {
                    report();
                }
            }
            for (; deletesSeen_ < context_->deletes().size(); ++deletesSeen_) {
                if (editing_)
                    eraseChars(context_->deletes()[deletesSeen_].second);
            }
            for (; commitsSeen_ < context_->commits().size(); ++commitsSeen_) {
                text_ += context_->commits()[commitsSeen_];
                editing_ = true;
            }
        }
        void eraseChars(unsigned int count) {
            for (; count > 0 && !text_.empty(); --count) {
                size_t start = text_.size();
                while (start > 0 && (static_cast<unsigned char>(text_[--start]) & 0xC0) == 0x80) {}
                text_.erase(start);
            }
        }
        void report() {
            const std::string text = editing_ ? text_ : "";
            context_->surroundingText().setText(text, fcitx::utf8::length(text), fcitx::utf8::length(text));
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 1);
        }

        TestInstance&                     testInstance_;
        fcitx::LotusEngine&               engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
        std::string                       cells_;
        size_t                            forwardedSeen_ = 0;
        size_t                            deletesSeen_   = 0;
        size_t                            commitsSeen_   = 0;
        bool                              editing_       = false;
        bool                              dialogOpened_  = false;
    };
} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-calc-text-input-cell-click");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");

    FakeCalc                calc(testInstance, engine, entry);
    // The first key in the next cell is a replacement in the old word, then one typed as is, then one
    // typed as is right after a tone mark Calc has not reported yet.
    calc.type("dduwowcj");
    calc.clickCell();
    calc.type("tieengs");
    calc.clickCell();
    calc.type("nam");
    calc.clickCell();
    calc.type("ddi");
    calc.clickCell();
    calc.type("veef");
    calc.clickCell();
    calc.type("nguwowif");
    const std::string expected = "được|tiếng|nam|đi|về|người";
    if (calc.dialogOpened() || calc.cells() != expected) {
        std::cerr << "Expected: " << expected << "\nActual: " << calc.cells() << (calc.dialogOpened() ? " (Delete Contents opened)" : "") << '\n';
        return 1;
    }
    return 0;
}
