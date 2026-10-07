// SPDX-License-Identifier: GPL-3.0-or-later
//
// On Wayland the IM sees no mouse click; the app only reports the cursor somewhere else in the same text.
#include "lotus-engine.h"
#include "test-input-context.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

namespace {
    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual) {
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
                    if (key.key().sym() == FcitxKey_BackSpace && !key.isRelease() && cursor_ > 0)
                        text_.erase(--cursor_, 1);
                }
                report();
                pumpEventLoop(testInstance_.instance, 60);
                if (context_->commits().size() > commits) {
                    for (size_t i = commits; i < context_->commits().size(); ++i)
                        insert(context_->commits()[i]);
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

        const std::string& text() const {
            return text_;
        }

      private:
        void insert(const std::string& s) {
            text_.insert(cursor_, s);
            cursor_ += s.size();
        }
        void report() {
            context_->surroundingText().setText(text_, cursor_, cursor_);
            context_->updateSurroundingText();
            pumpEventLoop(testInstance_.instance, 5);
        }

        TestInstance&                     testInstance_;
        fcitx::LotusEngine&               engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
        size_t                            cursor_ = 0;
    };

    bool expectText(const std::string& step, const FakeApp& app, const std::string& expected) {
        if (app.text() == expected)
            return true;
        reportFailure(step, "\"" + expected + "\"", "\"" + app.text() + "\"");
        return false;
    }
} // namespace

int main() {
    const std::string socketNamespace = "test-" + std::to_string(getpid());
    setenv("LOTUS_SOCKET_NAMESPACE", socketNamespace.c_str(), 1);
    configureTestPaths("fcitx5-lotus-cursor-jump-reset");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput");
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
    return 0;
}
