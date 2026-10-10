// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "key-sender-probe.h"
#include "test-input-context.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <sys/socket.h>

namespace {

    constexpr int kDefaultTimeoutMs = 5000;

    void          reportFailure(const std::string& step, const std::string& expected, const std::string& actual, const std::string& meaning) {
        std::cerr << "Step: " << step << '\n';
        std::cerr << "Expected: " << expected << '\n';
        std::cerr << "Actual: " << actual << '\n';
        std::cerr << "Meaning: " << meaning << '\n';
    }

    bool send(fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool requireAccepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() != requireAccepted) {
            reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(requireAccepted),
                          "accepted=" + std::to_string(event.accepted()) + ", commits=" + std::to_string(context.commits().size()),
                          "Uinput buffered-key handling accepted or rejected the key unexpectedly");
            return false;
        }
        return true;
    }

} // namespace

int main() {
    // Own a private socket name. Without this the listener below binds the same
    // abstract name a running fcitx5-lotus-server already holds, so the test
    // only passes on machines where the product is not running.

    configureTestPaths("fcitx5-ngosen-smooth-buffered-key-replay");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::NgoSenMode::Sen || engine.config().inputMethod.value() != "Telex") {
        reportFailure("configure Uinput/Telex", "mode=Uinput, input method=Telex", "configured mode or input method differs",
                      "the replay test cannot exercise Uinput Telex behavior");
        return 1;
    }

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);
    context->resetPreeditUpdateCount();

    // Telex a, s -> 'á'
    if (!send(engine, entry, *context, FcitxKey_a, false) || !send(engine, entry, *context, FcitxKey_s, true))
        return 1;
    int backspaces = 0;
    if (!listener.receive(backspaces, "the initial Telex replacement request did not arrive"))
        return 1;
    if (backspaces <= 0) {
        reportFailure("receive initial replacement count", "backspace count > 0", "backspace count=" + std::to_string(backspaces),
                      "the Telex replacement did not request deletion of the previous character");
        return 1;
    }

    if (!send(engine, entry, *context, FcitxKey_x, true) || !context->commits().empty()) {
        reportFailure("buffer key x before deletion completes", "no immediate commits", "commits=" + std::to_string(context->commits().size()),
                      "buffered key x was emitted before the first replacement completed");
        return 1;
    }
    for (int i = 0; i < backspaces; ++i) {
        if (!send(engine, entry, *context, FcitxKey_BackSpace, i + 1 == backspaces))
            return 1;
    }
    pumpEventLoop(testInstance.instance, 50); // the commit and the replay run from a timer

    int replayBackspaces = 0;
    if (!listener.receive(replayBackspaces, "buffered key was not replayed after deletion", "buffered x replay starts another replacement request within 5000 ms"))
        return 1;
    if (replayBackspaces <= 0) {
        reportFailure("receive replay replacement count", "backspace count > 0", "backspace count=" + std::to_string(replayBackspaces),
                      "buffered key x did not start another replacement after deletion");
        return 1;
    }
    for (int i = 0; i < replayBackspaces; ++i) {
        if (!send(engine, entry, *context, FcitxKey_BackSpace, i + 1 == replayBackspaces))
            return 1;
    }
    pumpEventLoop(testInstance.instance, 50);

    const std::vector<std::string> expected{"á", "ã"};
    if (context->commits() != expected) {
        std::string actual = "commits=";
        for (const auto& commit : context->commits())
            actual += "['" + commit + "']";
        const auto meaning = "the buffered key replay did not produce the expected final commits; "
                             "first-backspaces=" +
            std::to_string(backspaces) + ", replay-backspaces=" + std::to_string(replayBackspaces);
        reportFailure("verify final replay commits", "commits=['á']['ã']", actual, meaning);
        return 1;
    }
    return 0;
}