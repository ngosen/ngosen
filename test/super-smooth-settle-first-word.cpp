// SPDX-License-Identifier: GPL-3.0-or-later
//
// A just-emptied Messenger composer is still loading, so the first word of a message waits
// WaitSurroundingSettleFirstWordMs; a later word keeps the ordinary settle.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "key-sender-probe.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual) {
        std::cerr << "Step: " << step << "\nExpected: " << expected << "\nActual: " << actual << '\n';
    }

    std::string joinCommits(const TestInputContext& context) {
        std::string out;
        for (const auto& commit : context.commits())
            out += "['" + commit + "']";
        return out.empty() ? "(none)" : out;
    }

    void setSnapshot(TestInputContext& context, const std::string& text, unsigned int cursor) {
        context.surroundingText().setText(text, cursor, cursor);
        context.updateSurroundingText();
    }

    bool type(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool accepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() != accepted) {
            reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(accepted), "accepted=" + std::to_string(event.accepted()));
            return false;
        }
        return true;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-super-smooth-settle-first-word");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    // Covers the delete-then-commit fallback, not the default select-and-overtype path.
    config.setValueByPath("MessengerSelectOvertype", "False");
    config.setValueByPath("WaitSurroundingSettleMs", "20");
    // Far apart, so a stalled machine cannot make the first-word wait look like the ordinary one.
    config.setValueByPath("WaitSurroundingSettleFirstWordMs", "500");
    // The snapshot arrives a few ms into the wait for it; a stall must not let that wait time out first.
    config.setValueByPath("WaitSurroundingTimeoutMs", "5000");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::LotusMode::Sen || !engine.config().waitSurroundingEvent.value()) {
        reportFailure("configure Uinput", "mode=Uinput, WaitSurroundingEvent=True", "config differs");
        return 1;
    }

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    std::string shown;
    auto        typeLetters = [&](const std::string& letters) {
        for (char c : letters) {
            if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(c), false))
                return false;
            shown += c;
            setSnapshot(*context, shown + "\n\n", static_cast<unsigned int>(fcitx::utf8::length(shown)));
        }
        return true;
    };
    // Telex "w" on the u just typed: u -> ư, then the page shows the deletion done.
    auto replaceU = [&](const std::string& name) {
        if (!type(engine, entry, *context, FcitxKey_w, true))
            return false;
        int backspaces = 0;
        if (!listener.receive(backspaces))
            return false;
        if (backspaces != 2) {
            reportFailure("backspace count for " + name, "2", std::to_string(backspaces));
            return false;
        }
        for (int i = 0; i < backspaces; ++i) {
            if (!type(engine, entry, *context, FcitxKey_BackSpace, i + 1 == backspaces))
                return false;
        }
        pumpEventLoop(testInstance.instance, 3);
        shown.pop_back();
        setSnapshot(*context, shown + "\n\n", static_cast<unsigned int>(fcitx::utf8::length(shown)));
        return true;
    };

    setSnapshot(*context, "\n", 0);
    if (!typeLetters("chu") || !replaceU("first word u -> ư"))
        return 1;
    pumpEventLoop(testInstance.instance, 45);
    if (!context->commits().empty()) {
        reportFailure("first word: no commit 45 ms after the deletion shows done (first-word settle 500 ms)", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }
    pumpEventLoop(testInstance.instance, 600);
    if (context->commits() != std::vector<std::string>{"ư"}) {
        reportFailure("first word: commit once the first-word settle is over", "commits=['ư']", "commits=" + joinCommits(*context));
        return 1;
    }
    shown += "ư";
    setSnapshot(*context, shown + "\n\n", static_cast<unsigned int>(fcitx::utf8::length(shown)));

    if (!typeLetters(" cu") || !replaceU("second word u -> ư"))
        return 1;
    // Well under the first-word settle, so a commit here proves the ordinary one was used.
    pumpEventLoop(testInstance.instance, 150);
    if (context->commits() != std::vector<std::string>{"ư", "ư"}) {
        reportFailure("second word: commit within 150 ms (ordinary settle 20 ms)", "commits=['ư']['ư']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
