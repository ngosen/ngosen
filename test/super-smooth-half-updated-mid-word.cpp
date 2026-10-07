// SPDX-License-Identifier: GPL-3.0-or-later
//
// Same Messenger two-step update as super-smooth-half-updated-snapshot, but deleting more than one
// character: after the first backspace Edge reports the cursor between 'd' and 'o' while the text
// still holds both ("do\n\n", cursor 1). That snapshot must not count as done.
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
    configureTestPaths("fcitx5-lotus-super-smooth-half-updated-mid-word");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    // Covers the delete-then-commit fallback, not the default select-and-overtype path.
    config.setValueByPath("MessengerSelectOvertype", "False");
    // No settle delay: this test checks when the deletion counts as done.
    config.setValueByPath("WaitSurroundingSettleMs", "0");
    config.setValueByPath("WaitSurroundingSettleFirstWordMs", "0");
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
    setSnapshot(*context, "\n\n", 0);
    const std::string word = "do";
    for (size_t i = 0; i < word.size(); ++i) {
        if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(word[i]), false))
            return 1;
        setSnapshot(*context, word.substr(0, i + 1) + "\n\n", static_cast<unsigned int>(i + 1));
    }

    // Telex "d" again: do -> đo, two characters to delete plus the trigger backspace.
    if (!type(engine, entry, *context, FcitxKey_d, true))
        return 1;
    int backspaces = 0;
    if (!listener.receive(backspaces))
        return 1;
    if (backspaces != 3) {
        reportFailure("backspace count for do -> đo", "3", std::to_string(backspaces));
        return 1;
    }
    for (int i = 0; i < backspaces; ++i) {
        if (!type(engine, entry, *context, FcitxKey_BackSpace, i + 1 == backspaces))
            return 1;
    }

    // First backspace, half-updated: cursor between 'd' and 'o', both still in the text.
    pumpEventLoop(testInstance.instance, 3);
    setSnapshot(*context, "do\n\n", 1);
    pumpEventLoop(testInstance.instance, 3);
    if (!context->commits().empty()) {
        reportFailure("no commit with the cursor inside the word being deleted", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    // Second backspace, half-updated again: 'o' gone, cursor moved before the 'd'.
    setSnapshot(*context, "d\n\n", 0);
    pumpEventLoop(testInstance.instance, 3);
    if (!context->commits().empty()) {
        reportFailure("no commit with the cursor before the last deleted character", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    // The page catches up and the composer is empty: now the commit must go out.
    setSnapshot(*context, "\n", 0);
    pumpEventLoop(testInstance.instance, 30);
    if (context->commits() != std::vector<std::string>{"đo"}) {
        reportFailure("commit once the snapshot is consistent", "commits=['đo']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
