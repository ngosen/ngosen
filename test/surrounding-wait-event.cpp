// SPDX-License-Identifier: GPL-3.0-or-later
//
// After sending backspaces, Sen mode waits for the app to report the deletion and commits on that
// report, long before the timers would fire.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "key-sender-probe.h"
#include "test-input-context.h"

#include <cstddef>
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
    configureTestPaths("fcitx5-lotus-surrounding-wait-event");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    config.setValueByPath("WaitSurroundingSettleMs", "0");
    // Both timers far away, so only the report can end the wait in time.
    config.setValueByPath("WaitSurroundingMinPerKeyMs", "1000");
    config.setValueByPath("WaitSurroundingTimeoutMs", "5000");
    engine.setConfig(config);

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    setSnapshot(*context, "", 0);
    const std::string word = "tie";
    for (size_t i = 0; i < word.size(); ++i) {
        if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(word[i]), false))
            return 1;
        setSnapshot(*context, word.substr(0, i + 1), static_cast<unsigned int>(i + 1));
    }

    // Telex "e" again: e -> ê, one character to delete plus the trigger backspace.
    if (!type(engine, entry, *context, FcitxKey_e, true))
        return 1;
    int backspaces = 0;
    if (!listener.receive(backspaces) || backspaces != 2) {
        reportFailure("backspace count for e -> ê", "2", std::to_string(backspaces));
        return 1;
    }
    for (int i = 0; i < backspaces; ++i) {
        if (!type(engine, entry, *context, FcitxKey_BackSpace, i + 1 == backspaces))
            return 1;
    }

    pumpEventLoop(testInstance.instance, 3);
    if (!context->commits().empty()) {
        reportFailure("no commit before the app reports the deletion", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }
    setSnapshot(*context, "ti", 2);
    pumpEventLoop(testInstance.instance, 20);
    if (context->commits() != std::vector<std::string>{"ê"}) {
        reportFailure("commit on the report of the deletion", "commits=['ê']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
