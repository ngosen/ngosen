// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deleting then committing leaves a gap Facebook repaints into, so the Messenger composer selects the
// old characters with Shift+Left and types over the selection. The server is told to select by a
// negative count on the same socket.
// A field that never confirms the selection must NOT be typed into: the cursor is sitting N characters
// back, so a commit would scramble the text. Give the cursor back and drop the replacement instead,
// but still type the keys queued during the wait.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "key-sender-probe.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

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

    bool type(fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool accepted) {
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
    configureTestPaths("fcitx5-ngosen-super-smooth-messenger-select-overtype");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    config.setValueByPath("WaitSurroundingSettleMs", "40");
    config.setValueByPath("MessengerSelectOvertype", "True");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::NgoSenMode::Sen || !engine.config().waitSurroundingEvent.value()) {
        reportFailure("configure Uinput", "mode=Uinput, WaitSurroundingEvent=True", "config differs");
        return 1;
    }

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    auto setSelection = [&](const std::string& text, unsigned int cursor, unsigned int anchor) {
        context->surroundingText().setText(text, cursor, anchor);
        context->updateSurroundingText();
    };
    auto typeLetters = [&](const std::string& letters, const std::string& shownBefore, const std::string& tail) {
        std::string shown = shownBefore;
        for (char c : letters) {
            if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(c), false))
                return false;
            shown += c;
            setSnapshot(*context, shown + tail, static_cast<unsigned int>(fcitx::utf8::length(shown)));
        }
        return true;
    };

    // The Messenger composer: text, then the two newlines the page keeps after the cursor.
    setSnapshot(*context, "\n\n", 0);
    if (!typeLetters("chu", "", "\n\n"))
        return 1;

    // Telex "w": u -> ư. One character to replace, so the server is asked to select one.
    if (!type(engine, entry, *context, FcitxKey_w, true))
        return 1;
    int request = 0;
    if (!listener.receive(request))
        return 1;
    if (request != -1) {
        reportFailure("ask the server to select one character (negative count)", "-1", std::to_string(request));
        return 1;
    }
    // Edge keeps reporting the composer while it repaints; none of those is a selection.
    pumpEventLoop(testInstance.instance, 10);
    setSnapshot(*context, "chu\n\n", 3);
    pumpEventLoop(testInstance.instance, 10);
    setSnapshot(*context, "chu\n\n", 2);
    pumpEventLoop(testInstance.instance, 10);
    if (!context->commits().empty()) {
        reportFailure("no commit while the selection is unconfirmed", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    // Edge confirms: the same text, cursor still at 3, anchor one character back.
    setSelection("chu\n\n", 3, 2);
    pumpEventLoop(testInstance.instance, 10);
    if (context->commits() != std::vector<std::string>{"ư"}) {
        reportFailure("type over the selection once Edge confirms it", "commits=['ư']", "commits=" + joinCommits(*context));
        return 1;
    }
    setSnapshot(*context, "chư\n\n", 3);

    // A field that never confirms: after the deadline the cursor goes back and nothing is typed.
    const size_t forwardedBefore = context->forwarded().size();
    if (!type(engine, entry, *context, FcitxKey_x, true))
        return 1;
    if (!listener.receive(request) || request != -1) {
        reportFailure("ask the server to select one character for ư -> ữ", "-1", std::to_string(request));
        return 1;
    }
    // Keys typed while waiting are queued (the cursor is mid-selection) and must survive the fallback.
    pumpEventLoop(testInstance.instance, 10);
    if (!type(engine, entry, *context, FcitxKey_space, true) || !type(engine, entry, *context, FcitxKey_d, true))
        return 1;
    pumpEventLoop(testInstance.instance, 200);
    for (const auto& commit : context->commits()) {
        if (commit.find("ữ") != std::string::npos) {
            reportFailure("no overtype when the selection is never confirmed", "no commit of 'ữ'", "commits=" + joinCommits(*context));
            return 1;
        }
    }
    if (context->forwarded().size() <= forwardedBefore) {
        reportFailure("give the cursor back when the selection is never confirmed", "a forwarded key", "no forwarded key");
        return 1;
    }
    // Any other key would edit the text instead of moving the cursor.
    for (size_t i = forwardedBefore; i < context->forwarded().size(); ++i) {
        const auto& key = context->forwarded()[i].key();
        if (key.sym() != FcitxKey_Right || key.states() != fcitx::KeyStates()) {
            reportFailure("give the cursor back with Right", "only Right", "forwarded " + key.toString());
            return 1;
        }
    }
    {
        std::string typedBack;
        for (size_t i = 1; i < context->commits().size(); ++i)
            typedBack += context->commits()[i];
        if (context->commits().empty() || context->commits()[0] != "ư" || typedBack != " d") {
            reportFailure("keys typed during a never-confirmed wait are typed back", "commits=['ư'] then ' d'", "commits=" + joinCommits(*context));
            return 1;
        }
    }

    // Moving to another field: the typed-back "d" must not carry over as the start of the next word.
    {
        fcitx::InputContextEvent leave(context.get(), fcitx::EventType::InputContextFocusOut);
        engine.reset(entry, leave);
    }
    // The Facebook post composer, editing mid-text: the rest of the post follows the cursor, but the
    // field still ends in "\n\n", so it must select here too.
    const std::string postTail = "\n\nMình đã sửa gần hết\n\n";
    setSnapshot(*context, "Mời anh em " + postTail, 11);
    if (!typeLetters("cu", "Mời anh em ", postTail))
        return 1;
    if (!type(engine, entry, *context, FcitxKey_w, true))
        return 1;
    if (!listener.receive(request))
        return 1;
    if (request != -1) {
        reportFailure("select in the middle of a Facebook post", "-1", std::to_string(request));
        return 1;
    }
    setSelection("Mời anh em cu" + postTail, 13, 12);
    pumpEventLoop(testInstance.instance, 10);
    if (context->commits().empty() || context->commits().back() != "ư") {
        reportFailure("type over the selection in the middle of a Facebook post", "last commit 'ư'", "commits=" + joinCommits(*context));
        return 1;
    }
    {
        fcitx::InputContextEvent leave(context.get(), fcitx::EventType::InputContextFocusOut);
        engine.reset(entry, leave);
    }

    // A plain field does not end in "\n\n": it keeps deleting with backspaces.
    setSnapshot(*context, "cu", 2);
    if (!typeLetters("cu", "", ""))
        return 1;
    if (!type(engine, entry, *context, FcitxKey_w, true))
        return 1;
    if (!listener.receive(request)) {
        return 1;
    }
    if (request <= 0) {
        reportFailure("a plain field still deletes with backspaces", "a positive backspace count", std::to_string(request));
        return 1;
    }
    return 0;
}
