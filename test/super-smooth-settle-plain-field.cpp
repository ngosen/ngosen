// SPDX-License-Identifier: GPL-3.0-or-later
//
// WaitSurroundingSettleMs is for the Messenger composer only: its snapshot keeps "\n\n" right after
// the cursor. A plain field (no "\n\n" after the cursor) must get the commit as soon as the
// snapshot shows the deletion done, or every app pays the Messenger wait on each replacement.
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
    configureTestPaths("fcitx5-lotus-super-smooth-settle-plain-field");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    config.setValueByPath("WaitSurroundingSettleMs", "40");
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

    // Plain letters reach the page as real keys; the page reports each one.
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
    if (!listener.receive(backspaces))
        return 1;
    if (backspaces != 2) {
        reportFailure("backspace count for e -> ê", "2", std::to_string(backspaces));
        return 1;
    }
    for (int i = 0; i < backspaces; ++i) {
        if (!type(engine, entry, *context, FcitxKey_BackSpace, i + 1 == backspaces))
            return 1;
    }

    // A plain field reports the deletion done: commit now, WaitSurroundingSettleMs does not apply.
    pumpEventLoop(testInstance.instance, 3);
    setSnapshot(*context, "ti", 2);
    pumpEventLoop(testInstance.instance, 15);
    if (context->commits() != std::vector<std::string>{"ê"}) {
        reportFailure("commit within 15 ms in a field without the Messenger snapshot shape", "commits=['ê']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
