// SPDX-License-Identifier: GPL-3.0-or-later
//
// On an X11 session XTEST presses the keys a frontend cannot forward. Clients whose forwarding works keep it;
// Chromium-based clients, which report no surrounding text and whose address bar selects an inline
// autocompletion, select the old text with Shift+Left and type over it.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "ngosen-xtest.h"
#include "test-input-context.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

    std::string joinCounts(const std::vector<int>& counts) {
        std::string out;
        for (int count : counts)
            out += "[" + std::to_string(count) + "]";
        return out.empty() ? "(none)" : out;
    }

    uint64_t nowUs() {
        return fcitx::now(CLOCK_MONOTONIC);
    }

    // libuv timers count whole milliseconds from the loop's cached time; only lower bounds are checked.
    constexpr uint64_t ClockSlackUs = 3000;
    constexpr uint64_t SettleUs     = 50000;

    struct Harness {
        TestInstance&                     testInstance;
        fcitx::LotusEngine&               engine;
        fcitx::InputMethodEntry&          entry;
        std::vector<int>&                 sent;

        std::unique_ptr<TestInputContext> open(const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
            sent.clear();
            auto context = std::make_unique<TestInputContext>(&testInstance.instance, program, frontend);
            context->setCapabilityFlags(caps);
            context->focusIn();
            fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, focus);
            return context;
        }

        // Feeds a key as the X server would deliver it back to the input method.
        bool key(TestInputContext& context, fcitx::Key key, bool release, bool accepted) {
            fcitx::KeyEvent event(&context, key, release);
            engine.keyEvent(entry, event);
            if (event.accepted() != accepted) {
                reportFailure("key " + key.toString() + (release ? " up" : " down"), "accepted=" + std::to_string(accepted), "accepted=" + std::to_string(event.accepted()));
                return false;
            }
            return true;
        }

        // Types "tie", then the second e that replaces it with "tiê".
        bool typeTieE(TestInputContext& context) {
            for (char c : std::string("tie")) {
                if (!key(context, fcitx::Key(static_cast<fcitx::KeySym>(c)), false, false))
                    return false;
            }
            pumpEventLoop(testInstance.instance, 1);
            return key(context, fcitx::Key(FcitxKey_e), false, true);
        }

        // Chromium on X11 drops and regains focus right after our selection keys.
        void bounceFocus(TestInputContext& context) {
            fcitx::InputContextEvent out(&context, fcitx::EventType::InputContextFocusOut);
            engine.reset(entry, out);
            engine.deactivate(entry, out);
            fcitx::InputContextEvent in(&context, fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, in);
        }

        template <typename Done>
        uint64_t pumpUntil(Done done, uint64_t deadlineUs) {
            while (!done() && nowUs() < deadlineUs)
                pumpEventLoop(testInstance.instance, 1);
            return nowUs();
        }
    };

    // The Chromium address bar selects an inline autocompletion after the typed text, so a BackSpace
    // would only remove that. Widening the selection with Shift+Left and typing over it replaces both.
    bool chromiumSelectsAndOvertypes(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps, bool bounceFocus = false) {
        const std::string where   = frontend + " " + program + (bounceFocus ? " with focus bounce" : "") + ": ";
        auto              context = h.open(program, frontend, caps);
        if (!h.typeTieE(*context))
            return false;
        if (h.sent != std::vector<int>{-1}) {
            reportFailure(where + "select one character through XTEST", "[-1]", joinCounts(h.sent));
            return false;
        }
        if (!context->forwarded().empty()) {
            reportFailure(where + "no forwarded key", "(none)", std::to_string(context->forwarded().size()) + " keys");
            return false;
        }
        // The selection keys come back through the input method and must reach the app.
        const auto shiftLeft = fcitx::Key(FcitxKey_Left, fcitx::KeyStates(fcitx::KeyState::Shift));
        if (!h.key(*context, fcitx::Key(FcitxKey_Shift_R), false, false) || !h.key(*context, shiftLeft, false, false) || !h.key(*context, shiftLeft, true, false))
            return false;
        pumpEventLoop(h.testInstance.instance, 5);
        if (!context->commits().empty()) {
            reportFailure(where + "no commit while Shift is still down", "(none)", joinCommits(*context));
            return false;
        }
        if (!h.key(*context, fcitx::Key(FcitxKey_Shift_R, fcitx::KeyStates(fcitx::KeyState::Shift)), true, false))
            return false;
        const uint64_t releasedAt = nowUs();
        if (bounceFocus)
            h.bounceFocus(*context);
        const uint64_t committedAt = h.pumpUntil([&] { return !context->commits().empty(); }, releasedAt + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(where + "type ê over the selection", "['ê']", joinCommits(*context));
            return false;
        }
        // Chromium asks the input method about each key before it handles the key, so the Left presses
        // may still be pending when the Shift release comes back.
        if (committedAt - releasedAt + ClockSlackUs < SettleUs) {
            reportFailure(where + "commit at least 50 ms after Shift is released", ">= 50000 us", std::to_string(committedAt - releasedAt) + " us");
            return false;
        }
        return true;
    }

    // SDL takes neither forwarded keys nor surrounding text; XTEST presses BackSpace like a keyboard, and
    // the last one comes back to the input method as the sentinel.
    bool sdlPressesBackSpaceThroughXTest(Harness& h) {
        auto context = h.open("Medieval2", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit});
        if (!h.typeTieE(*context))
            return false;
        if (h.sent != std::vector<int>{2}) {
            reportFailure("SDL: press one BackSpace plus the sentinel through XTEST", "[2]", joinCounts(h.sent));
            return false;
        }
        if (!h.key(*context, fcitx::Key(FcitxKey_BackSpace), false, false) || !h.key(*context, fcitx::Key(FcitxKey_BackSpace), false, true))
            return false;
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("SDL: commit once the sentinel is back", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // Clients whose forwarded keys already work keep forwarding.
    bool keepsForwarding(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        const std::string where   = frontend + " " + program + ": ";
        auto              context = h.open(program, frontend, caps);
        if (!h.typeTieE(*context))
            return false;
        pumpEventLoop(h.testInstance.instance, 5);
        if (!h.sent.empty()) {
            reportFailure(where + "no XTEST key", "(none)", joinCounts(h.sent));
            return false;
        }
        const auto& forwarded = context->forwarded();
        if (forwarded.size() != 2 || forwarded[0].key().sym() != FcitxKey_BackSpace) {
            reportFailure(where + "forward the backspace", "[BackSpace down][BackSpace up]", std::to_string(forwarded.size()) + " keys");
            return false;
        }
        return true;
    }

} // namespace

int main() {
    setenv("XDG_CURRENT_DESKTOP", "XFCE", 1);
    setenv("DISPLAY", ":0", 1);
    unsetenv("WAYLAND_DISPLAY");

    std::vector<int> sent;
    setXTestSenderForTest([&sent](int count) {
        sent.push_back(count);
        return true;
    });

    configureTestPaths("ngosen-x11-xtest-replacement");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    engine.setConfig(config);

    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
    Harness                 h{testInstance, engine, entry, sent};

    const auto              chromeCaps = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::KeyEventOrderFix};
    if (!chromiumSelectsAndOvertypes(h, "microsoft-edge", "dbus", chromeCaps))
        return 1;
    if (!chromiumSelectsAndOvertypes(h, "microsoft-edge", "dbus", chromeCaps, true))
        return 1;
    if (!chromiumSelectsAndOvertypes(h, "chromium", "fcitx4", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit}))
        return 1;
    if (!sdlPressesBackSpaceThroughXTest(h))
        return 1;
    // VTE terminals report no surrounding text either, but print Shift+Left as an escape sequence.
    if (!keepsForwarding(h, "gnome-terminal-server", "dbus", chromeCaps))
        return 1;
    if (!keepsForwarding(h, "xfce4-terminal", "dbus", chromeCaps))
        return 1;
    if (!keepsForwarding(h, "gtk3app", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix}))
        return 1;
    if (!keepsForwarding(h, "geany", "xim", fcitx::CapabilityFlags{}))
        return 1;
    setXTestSenderForTest({});
    return 0;
}
