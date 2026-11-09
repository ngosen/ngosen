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
                          "Sen buffered-key handling accepted or rejected the key unexpectedly");
            return false;
        }
        return true;
    }

} // namespace

namespace {

    std::string joined(const std::vector<std::string>& commits, size_t from) {
        std::string out;
        for (size_t i = from; i < commits.size(); ++i)
            out += "['" + commits[i] + "']";
        return out;
    }

    // Types a, s, then x while the deletion for 'á' is still out, and finishes every deletion asked for.
    bool replayBufferedKey(fcitx::Instance& instance, fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, KeySenderProbe& listener,
                           bool fold) {
        fcitx::RawConfig config;
        config.setValueByPath("Mode", "Sen");
        config.setValueByPath("VietnameseOn", "True");
        config.setValueByPath("InputMethod", "Telex");
        config.setValueByPath("FoldReplayReplacement", fold ? "True" : "False");
        engine.setConfig(config);
        const size_t firstCommit  = context.commits().size();
        const int    firstRequest = listener.requests();
        if (!send(engine, entry, context, FcitxKey_a, false) || !send(engine, entry, context, FcitxKey_s, true))
            return false;
        int backspaces = 0;
        if (!listener.receive(backspaces, "the initial Telex replacement request did not arrive"))
            return false;
        if (backspaces <= 0) {
            reportFailure("receive initial replacement count", "backspace count > 0", "backspace count=" + std::to_string(backspaces),
                          "the Telex replacement did not request deletion of the previous character");
            return false;
        }
        if (!send(engine, entry, context, FcitxKey_x, true) || context.commits().size() != firstCommit) {
            reportFailure("buffer key x before deletion completes", "no immediate commits", "commits=" + joined(context.commits(), firstCommit),
                          "buffered key x was emitted before the first replacement completed");
            return false;
        }
        for (int i = 0; i < backspaces; ++i) {
            if (!send(engine, entry, context, FcitxKey_BackSpace, i + 1 == backspaces))
                return false;
        }
        pumpEventLoop(instance, 50); // the commit and the replay run from a timer

        if (fold) {
            // The replay changes 'á' to 'ã' before sending it, so nothing more is deleted.
            if (listener.requests() != firstRequest + 1 || joined(context.commits(), firstCommit) != "['ã']") {
                reportFailure("fold the replayed replacement", "one deletion, commits=['ã']",
                              "deletions=" + std::to_string(listener.requests() - firstRequest) + ", commits=" + joined(context.commits(), firstCommit),
                              "the replay sent 'á' and deleted it again instead of sending 'ã'");
                return false;
            }
            return true;
        }
        int replayBackspaces = 0;
        if (!listener.receive(replayBackspaces, "buffered key was not replayed after deletion", "buffered x replay starts another replacement request"))
            return false;
        if (replayBackspaces <= 0) {
            reportFailure("receive replay replacement count", "backspace count > 0", "backspace count=" + std::to_string(replayBackspaces),
                          "buffered key x did not start another replacement after deletion");
            return false;
        }
        for (int i = 0; i < replayBackspaces; ++i) {
            if (!send(engine, entry, context, FcitxKey_BackSpace, i + 1 == replayBackspaces))
                return false;
        }
        pumpEventLoop(instance, 50);
        if (joined(context.commits(), firstCommit) != "['á']['ã']") {
            reportFailure("verify final replay commits", "commits=['á']['ã']", "commits=" + joined(context.commits(), firstCommit),
                          "the buffered key replay did not produce the expected final commits; first-backspaces=" + std::to_string(backspaces) +
                              ", replay-backspaces=" + std::to_string(replayBackspaces));
            return false;
        }
        return true;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-smooth-buffered-key-replay");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::NgoSenMode::Sen || engine.config().inputMethod.value() != "Telex") {
        reportFailure("configure Sen/Telex", "mode=Sen, input method=Telex", "configured mode or input method differs", "the replay test cannot exercise Sen Telex behavior");
        return 1;
    }

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);
    context->resetPreeditUpdateCount();

    if (!replayBufferedKey(testInstance.instance, engine, entry, *context, listener, false))
        return 1;
    const int       requestsBeforeSpace = listener.requests();
    fcitx::KeyEvent space(context.get(), fcitx::Key(FcitxKey_space), false);
    engine.keyEvent(entry, space);
    pumpEventLoop(testInstance.instance, 50);
    // The second run reads deletion requests in order, so the space must not add one.
    if (listener.requests() != requestsBeforeSpace) {
        reportFailure("end the word with a space", "no deletion request", "requests=" + std::to_string(listener.requests() - requestsBeforeSpace),
                      "the space between the runs asked for a deletion");
        return 1;
    }
    if (!replayBufferedKey(testInstance.instance, engine, entry, *context, listener, true))
        return 1;
    return 0;
}
