// SPDX-License-Identifier: GPL-3.0-or-later
//
// In a Vim-style editor the letters of a command such as "ciw" reach the engine before the word typed
// in insert mode, since a terminal reports no text. They made the word invalid, so it stayed raw until
// the next space (#302). After Escape the engine commits the commands as typed and starts the word again.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

    int  failures = 0;

    void check(const std::string& step, const std::string& expected, const std::string& actual) {
        if (actual != expected) {
            std::cerr << "FAIL: " << step << ": expected [" << expected << "], got [" << actual << "]\n";
            ++failures;
        }
    }

    // Plays a terminal: no surrounding text, forwarded backspaces and commits apply to its text.
    class FakeApp {
      public:
        FakeApp(TestInstance& testInstance, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry) :
            testInstance_(testInstance), engine_(engine), entry_(entry), context_(std::make_unique<TestInputContext>(&testInstance.instance, "foot", "wayland")) {
            focusIn();
        }

        void key(fcitx::Key key, const std::string& text = "") {
            const size_t    forwarded = context_->forwarded().size();
            const size_t    commits   = context_->commits().size();
            fcitx::KeyEvent event(context_.get(), key, false);
            engine_.keyEvent(entry_, event);
            if (!event.accepted())
                text_ += text;
            for (size_t i = forwarded; i < context_->forwarded().size(); ++i) {
                const auto& k = context_->forwarded()[i];
                if (k.key().sym() == FcitxKey_BackSpace && !k.isRelease())
                    erase();
            }
            pumpEventLoop(testInstance_.instance, 30);
            for (size_t i = commits; i < context_->commits().size(); ++i)
                text_ += context_->commits()[i];
        }

        void type(const std::string& keys) {
            for (char c : keys)
                key(fcitx::Key(static_cast<fcitx::KeySym>(c)), std::string(1, c));
        }

        void escape() {
            key(fcitx::Key(FcitxKey_Escape));
        }

        void ctrlBracket() {
            key(fcitx::Key(FcitxKey_bracketleft, fcitx::KeyState::Ctrl));
        }

        void enter() {
            key(fcitx::Key(FcitxKey_Return), "\n");
        }

        void focusOutAndIn() {
            fcitx::InputContextEvent out(context_.get(), fcitx::EventType::InputContextFocusOut);
            engine_.reset(entry_, out);
            engine_.deactivate(entry_, out);
            context_->focusOut();
            focusIn();
        }

        const std::string& text() const {
            return text_;
        }

      private:
        void focusIn() {
            context_->focusIn();
            fcitx::InputContextEvent focus(context_.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry_, focus);
        }
        void erase() {
            size_t start = text_.size();
            while (start > 0 && (static_cast<unsigned char>(text_[--start]) & 0xC0) == 0x80) {}
            text_.erase(start);
        }

        TestInstance&                     testInstance_;
        fcitx::NgoSenEngine&              engine_;
        const fcitx::InputMethodEntry&    entry_;
        std::unique_ptr<TestInputContext> context_;
        std::string                       text_;
    };

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-vim-insert-first-word");
    TestInstance            testInstance;
    fcitx::NgoSenEngine     engine(&testInstance.instance);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::RawConfig        config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    // Insert commands, with motions before them, then the first word typed in insert mode.
    for (const char* command : {"i", "a", "o", "A", "s", "S", "cc", "cw", "ciw", "ea", "xi", "jjkciw"}) {
        FakeApp app(testInstance, engine, entry);
        app.type("xin chao ");
        app.escape();
        app.type(command);
        app.type("tieengs vieetj");
        check(std::string("Escape, ") + command + ", tieengs vieetj", std::string("xin chao ") + command + "tiếng việt", app.text());
    }
    {
        FakeApp app(testInstance, engine, entry);
        app.escape();
        app.type("ianhs em");
        check("Escape, i, a word starting with a vowel", "iánh em", app.text());
    }
    {
        FakeApp app(testInstance, engine, entry);
        app.ctrlBracket();
        app.type("ciwtieengs");
        check("Ctrl+[ leaves insert mode like Escape", "ciwtiếng", app.text());
    }
    // ":w" and Enter are typed between Escape and the next command.
    {
        FakeApp app(testInstance, engine, entry);
        app.escape();
        app.type(":w");
        app.enter();
        app.type("ciwtieengs");
        check("Escape, :w, Enter, ciw, tieengs", ":w\nciwtiếng", app.text());
    }
    // An English word after a command gets what it gets when typed fresh.
    for (const char* word : {"shows", "knows", "free", "address", "return", "software"}) {
        FakeApp app(testInstance, engine, entry);
        app.escape();
        app.type("o");
        app.type(std::string(word) + " ");
        check(std::string("Escape, o, ") + word, std::string("o") + word + " ", app.text());
    }
    // Without Escape the keys are a word, and an invalid word stays raw as before.
    {
        FakeApp app(testInstance, engine, entry);
        app.type("ciwtieengs");
        check("ciwtieengs typed without Escape", "ciwtieengs", app.text());
    }
    // The commands are expected only until a Vietnamese word shows the user is typing text again.
    {
        FakeApp app(testInstance, engine, entry);
        app.escape();
        app.type("otieengs ciwtieengs");
        check("Escape, o, tieengs, then ciwtieengs as text", "otiếng ciwtieengs", app.text());
    }
    // Another window may take the keys after Escape.
    {
        FakeApp app(testInstance, engine, entry);
        app.escape();
        app.focusOutAndIn();
        app.type("ciwtieengs");
        check("Escape, focus out and in, ciwtieengs", "ciwtieengs", app.text());
    }
    return failures == 0 ? 0 : 1;
}
