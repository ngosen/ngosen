// SPDX-License-Identifier: GPL-3.0-or-later
//
// Keys typed while a replacement is pending are replayed after it. Under GNOME, mutter sends one
// text-input "done" per main-loop turn and the client keeps only the last commit_string before it, so
// committing the replacement and the replayed keys separately loses the replacement: typing "ddi"
// fast in the Facebook composer showed "i" instead of "đi". They must go out as one commit.
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
    configureTestPaths("fcitx5-lotus-replay-single-commit");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    config.setValueByPath("MessengerSelectOvertype", "True");
    engine.setConfig(config);

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    // The Facebook composer: empty text followed by the two newlines the page keeps after the cursor.
    setSnapshot(*context, "\n\n", 0);
    if (!type(engine, entry, *context, FcitxKey_d, false))
        return 1;
    setSnapshot(*context, "d\n\n", 1);

    // Telex "dd": d -> đ, selected and typed over.
    if (!type(engine, entry, *context, FcitxKey_d, true))
        return 1;
    int request = 0;
    if (!listener.receive(request) || request != -1) {
        reportFailure("ask the server to select one character", "-1", std::to_string(request));
        return 1;
    }
    // "i" arrives before the field confirms the selection, so it is queued.
    if (!type(engine, entry, *context, FcitxKey_i, true))
        return 1;
    if (!context->commits().empty()) {
        reportFailure("nothing committed before the selection is confirmed", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    context->surroundingText().setText("d\n\n", 1, 0);
    context->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 10);
    if (context->commits() != std::vector<std::string>{"đi"}) {
        reportFailure("replacement and replayed key go out as one commit", "commits=['đi']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
