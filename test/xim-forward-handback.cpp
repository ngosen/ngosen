// SPDX-License-Identifier: GPL-3.0-or-later
//
// An XIM client sometimes hands a forwarded backspace back unprocessed. It must reach
// the client again and the commit must wait for it, or the new text lands before the deletion.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <cstdint>
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

    bool type(fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool accepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() != accepted) {
            reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(accepted), "accepted=" + std::to_string(event.accepted()));
            return false;
        }
        return true;
    }

    std::string describeForwarded(const TestInputContext& context) {
        std::string out;
        for (const auto& event : context.forwarded())
            out += "[" + event.key().toString() + (event.isRelease() ? " up" : " down") + "]";
        return out.empty() ? "(none)" : out;
    }

    uint64_t nowUs() {
        return fcitx::now(CLOCK_MONOTONIC);
    }

    // libuv timers count whole milliseconds from the loop's cached time, so a commit may land up to
    // this much before its deadline. Only lower bounds on time are checked.
    constexpr uint64_t ClockSlackUs = 3000;
    constexpr uint64_t WaitUs       = 15000;

    struct Harness {
        fcitx::Instance&               instance;
        fcitx::NgoSenEngine&           engine;
        const fcitx::InputMethodEntry& entry;
        TestInputContext&              context;

        bool                           type(fcitx::KeySym symbol, bool accepted) {
            return ::type(engine, entry, context, symbol, accepted);
        }
        // Pumps 1 ms at a time until done() or the deadline; returns the time the loop stopped.
        template <typename Done>
        uint64_t pumpUntil(Done done, uint64_t deadlineUs) {
            while (!done() && nowUs() < deadlineUs)
                pumpEventLoop(instance, 1);
            return nowUs();
        }
    };

    enum class Outcome {
        Pass,
        Fail,
        Stalled
    };

    // Types "tie" + "e" and hands the forwarded backspace back about 8 ms later.
    Outcome handBackOnce(Harness& h, bool firstWord) {
        if (!firstWord && !h.type(FcitxKey_space, true))
            return Outcome::Fail;
        for (const char key : std::string("tie")) {
            if (!h.type(static_cast<fcitx::KeySym>(key), false))
                return Outcome::Fail;
        }
        const size_t commitsBefore   = h.context.commits().size();
        const size_t forwardedBefore = h.context.forwarded().size();
        const auto   committed       = [&] { return h.context.commits().size() > commitsBefore; };
        // libuv dates a new timer from the loop's last run, and a real key arrives inside one.
        pumpEventLoop(h.instance, 1);
        const uint64_t replacedAt = nowUs();
        if (!h.type(FcitxKey_e, true))
            return Outcome::Fail;
        // The client is still waiting for the reply to that key; nothing may reach it before the reply.
        if (h.context.forwarded().size() != forwardedBefore) {
            reportFailure("no backspace forwarded inside the key event", "(none)", describeForwarded(h.context));
            return Outcome::Fail;
        }
        // The forward is due before the commit, so nothing else can run first.
        h.pumpUntil([&] { return h.context.forwarded().size() > forwardedBefore; }, replacedAt + 500000);
        const auto& forwarded = h.context.forwarded();
        if (forwarded.size() != forwardedBefore + 2 || forwarded[forwardedBefore].key().sym() != FcitxKey_BackSpace || forwarded[forwardedBefore].isRelease() ||
            !forwarded.back().isRelease()) {
            reportFailure("forward the backspace for e -> ê after the reply", "[BackSpace down][BackSpace up]", describeForwarded(h.context));
            return Outcome::Fail;
        }
        const uint64_t stoppedAt = h.pumpUntil(committed, replacedAt + 8000);
        if (committed()) {
            if (stoppedAt - replacedAt + ClockSlackUs < WaitUs) {
                reportFailure("no commit before the wait ends", ">= " + std::to_string(WaitUs) + " us", std::to_string(stoppedAt - replacedAt) + " us");
                return Outcome::Fail;
            }
            return Outcome::Stalled; // the machine stalled past the wait before the hand-back
        }
        const uint64_t handedBackAt = nowUs();
        if (!h.type(FcitxKey_BackSpace, false))
            return Outcome::Fail;
        if (committed()) {
            reportFailure("no commit when the backspace comes back", "no new commit", "commits=" + joinCommits(h.context));
            return Outcome::Fail;
        }
        const uint64_t committedAt = h.pumpUntil(committed, handedBackAt + 500000);
        if (!committed() || h.context.commits().back() != "ê") {
            reportFailure("commit after the handed-back backspace", "last commit 'ê'", "commits=" + joinCommits(h.context));
            return Outcome::Fail;
        }
        if (committedAt - handedBackAt + ClockSlackUs < WaitUs) {
            reportFailure("commit a full wait after the handed-back backspace", ">= " + std::to_string(WaitUs) + " us", std::to_string(committedAt - handedBackAt) + " us");
            return Outcome::Fail;
        }
        return Outcome::Pass;
    }

    // Without a hand-back the commit still waits the full time, long enough for one on its way.
    bool waitWithoutHandBack(Harness& h) {
        if (!h.type(FcitxKey_space, true) || !h.type(FcitxKey_a, false))
            return false;
        const size_t commitsBefore = h.context.commits().size();
        // libuv dates a new timer from the loop's last run, and a real key arrives inside one.
        pumpEventLoop(h.instance, 1);
        const uint64_t replacedAt = nowUs();
        if (!h.type(FcitxKey_s, true))
            return false;
        const uint64_t committedAt = h.pumpUntil([&] { return h.context.commits().size() > commitsBefore; }, replacedAt + 500000);
        if (h.context.commits().size() != commitsBefore + 1 || h.context.commits().back() != "á") {
            reportFailure("commit a -> á", "last commit 'á'", "commits=" + joinCommits(h.context));
            return false;
        }
        if (committedAt - replacedAt + ClockSlackUs < WaitUs) {
            reportFailure("commit a full wait after the forward", ">= " + std::to_string(WaitUs) + " us", std::to_string(committedAt - replacedAt) + " us");
            return false;
        }
        return true;
    }

} // namespace

int main() {
    // A plain X11 session: XIM forwards there as well as under XWayland.
    unsetenv("WAYLAND_DISPLAY");

    configureTestPaths("fcitx5-ngosen-xim-forward-handback");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    // XIM clients declare no surrounding text, so the wait is timer-only.
    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "test", "xim");
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    Harness h{testInstance.instance, engine, entry, *context};
    // A machine that stalls past the wait before the hand-back proves nothing; try another word.
    Outcome outcome = Outcome::Stalled;
    for (int attempt = 0; attempt < 5 && outcome == Outcome::Stalled; ++attempt)
        outcome = handBackOnce(h, attempt == 0);
    if (outcome == Outcome::Stalled) {
        reportFailure("hand the backspace back before the commit", "one of 5 attempts", "machine stalled every time");
        return 1;
    }
    if (outcome == Outcome::Fail || !waitWithoutHandBack(h))
        return 1;
    return 0;
}
