// SPDX-License-Identifier: GPL-3.0-or-later
//
// On an X11 session XTEST presses the keys a frontend cannot forward, and the backspaces for XIM clients.
// Clients whose forwarding works keep it; the address bar of Chromium-based clients, which report no surrounding text, selects an inline
// autocompletion, so there the old text is selected with Shift+Left and typed over. Under Xwayland, XIM clients get
// their backspaces as key events sent to their window.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
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
        fcitx::NgoSenEngine&              engine;
        fcitx::InputMethodEntry&          entry;
        std::vector<int>&                 sent;

        std::unique_ptr<TestInputContext> open(const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps, int caretWidth = 0) {
            sent.clear();
            auto context = std::make_unique<TestInputContext>(&testInstance.instance, program, frontend);
            context->setCapabilityFlags(caps);
            context->setCursorRect(fcitx::Rect(100, 80, 100 + caretWidth, 97));
            context->focusIn();
            fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, focus);
            return context;
        }

        // Feeds a key as the X server would deliver it back to the input method.
        bool key(TestInputContext& context, fcitx::Key key, bool release, bool accepted, int time = 0) {
            fcitx::KeyEvent event(&context, key, release, time);
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
    bool chromiumSelectsAndOvertypes(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps, int bounceAfterMs = -1, int caretWidth = 1) {
        const std::string where   = frontend + " " + program + " with focus bounce after " + std::to_string(bounceAfterMs) + " ms: ";
        auto              context = h.open(program, frontend, caps, caretWidth);
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
        if (bounceAfterMs >= 0) {
            h.pumpUntil([] { return false; }, releasedAt + bounceAfterMs * 1000ULL);
            h.bounceFocus(*context);
        }
        const uint64_t committedAt = h.pumpUntil([&] { return !context->commits().empty(); }, releasedAt + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(where + "type ê over the selection", "['ê']", joinCommits(*context));
            return false;
        }
        if (!context->forwarded().empty()) {
            reportFailure(where + "no backspace in the address bar", "(none)", std::to_string(context->forwarded().size()) + " keys");
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

    bool isTap(const std::vector<fcitx::ForwardKeyEvent>& keys, size_t at, fcitx::KeySym sym) {
        return keys.size() >= at + 2 && keys[at].key().sym() == sym && keys[at + 1].key().sym() == sym;
    }

    // Google Docs and Slides report a thin caret like the address bar, but keep focus when Shift+Left
    // selects, and a commit then does not replace the selection (#516).
    bool fieldKeepingFocusDeletesWithBackSpace(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        const std::string where   = frontend + " " + program + " keeping focus: ";
        auto              context = h.open(program, frontend, caps, 2);
        if (!h.typeTieE(*context))
            return false;
        const auto shiftLeft = fcitx::Key(FcitxKey_Left, fcitx::KeyStates(fcitx::KeyState::Shift));
        if (!h.key(*context, fcitx::Key(FcitxKey_Shift_R), false, false) || !h.key(*context, shiftLeft, false, false) || !h.key(*context, shiftLeft, true, false) ||
            !h.key(*context, fcitx::Key(FcitxKey_Shift_R, fcitx::KeyStates(fcitx::KeyState::Shift)), true, false))
            return false;
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 800000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(where + "commit ê", "['ê']", joinCommits(*context));
            return false;
        }
        if (context->forwarded().size() != 4 || !isTap(context->forwarded(), 0, FcitxKey_Right) || !isTap(context->forwarded(), 2, FcitxKey_BackSpace)) {
            reportFailure(where + "drop the selection, then delete with a backspace", "[Right][BackSpace]", std::to_string(context->forwarded().size()) + " keys");
            return false;
        }
        // The field has answered: the next replacement goes straight to backspaces.
        h.sent.clear();
        if (!h.key(*context, fcitx::Key(FcitxKey_s), false, true))
            return false;
        h.pumpUntil([&] { return context->commits().size() >= 2; }, nowUs() + 800000);
        if (!h.sent.empty() || context->commits().size() != 2 || context->commits()[1] != "íê" || context->forwarded().size() != 8) {
            reportFailure(where + "no second selection", "no XTEST, íê after two more backspaces", joinCounts(h.sent) + ", " + joinCommits(*context));
            return false;
        }
        // Focus may now be in another field, so ask again.
        h.bounceFocus(*context);
        if (!h.key(*context, fcitx::Key(FcitxKey_space), false, false) || !h.typeTieE(*context))
            return false;
        if (h.sent != std::vector<int>{-1}) {
            reportFailure(where + "select again after focus moved", "[-1]", joinCounts(h.sent));
            return false;
        }
        return true;
    }

    // XTEST presses BackSpace like a keyboard, and the last one comes back to the input method as the
    // sentinel. SDL takes neither forwarded keys nor surrounding text; XIM clients can hand a forwarded key
    // back after the commit.
    bool pressesBackSpaceThroughXTest(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps,
                                      fcitx::KeyStates sentinelStates = fcitx::KeyStates()) {
        const std::string where   = frontend + " " + program + ": ";
        auto              context = h.open(program, frontend, caps);
        if (!h.typeTieE(*context))
            return false;
        if (h.sent != std::vector<int>{2} || !context->forwarded().empty()) {
            reportFailure(where + "press one BackSpace plus the sentinel through XTEST, forward nothing", "[2], 0 forwarded",
                          joinCounts(h.sent) + ", " + std::to_string(context->forwarded().size()) + " forwarded");
            return false;
        }
        // XTEST sends the presses in one burst, so the X server gives them the same time.
        constexpr int CodeBackSpace = 22;
        constexpr int BurstTime     = 5000;
        const auto    backSpace     = fcitx::Key(FcitxKey_BackSpace, fcitx::KeyStates(), CodeBackSpace);
        const auto    sentinel      = fcitx::Key(FcitxKey_BackSpace, sentinelStates, CodeBackSpace);
        if (!h.key(*context, backSpace, false, false, BurstTime) || !h.key(*context, sentinel, false, true, BurstTime))
            return false;
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(where + "commit once the sentinel is back", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    bool commitsWhenBackspacesFail(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        auto context = h.open(program, frontend, caps);
        if (!h.typeTieE(*context))
            return false;
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(frontend + " " + program + ": commit when the backspaces cannot be sent", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // Clients whose forwarded keys already work keep forwarding.
    bool keepsForwarding(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps, int caretWidth = 0) {
        const std::string where   = frontend + " " + program + ": ";
        auto              context = h.open(program, frontend, caps, caretWidth);
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

    bool erasesWithCommittedDel(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        const std::string where   = frontend + " " + program + ": ";
        auto              context = h.open(program, frontend, caps);
        if (!h.typeTieE(*context))
            return false;
        pumpEventLoop(h.testInstance.instance, 5);
        if (!h.sent.empty() || !context->forwarded().empty()) {
            reportFailure(where + "no key sent", "(none)", joinCounts(h.sent) + ", " + std::to_string(context->forwarded().size()) + " forwarded");
            return false;
        }
        if (context->commits() != std::vector<std::string>{"\x7fê"}) {
            reportFailure(where + "one commit erases e and types ê", "['<DEL>ê']", joinCommits(*context));
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
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    engine.setConfig(config);

    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    Harness                 h{testInstance, engine, entry, sent};

    const auto              chromeCaps = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::KeyEventOrderFix};
    if (!chromiumSelectsAndOvertypes(h, "microsoft-edge", "dbus", chromeCaps, 0))
        return 1;
    // A busy machine delays the focus change past the usual settling time.
    if (!chromiumSelectsAndOvertypes(h, "microsoft-edge", "dbus", chromeCaps, 100))
        return 1;
    if (!chromiumSelectsAndOvertypes(h, "chromium", "fcitx4", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit}))
        return 1;
    // Vivaldi's address bar is a web page field, which keeps focus while Shift+Left selects.
    if (!chromiumSelectsAndOvertypes(h, "vivaldi-bin", "dbus", chromeCaps, -1, 73))
        return 1;
    // Google Docs in Vivaldi reports a thin caret, and a commit there does not replace a selection.
    if (!keepsForwarding(h, "vivaldi-bin", "dbus", chromeCaps, 2))
        return 1;
    if (!fieldKeepingFocusDeletesWithBackSpace(h, "microsoft-edge", "dbus", chromeCaps))
        return 1;
    if (!pressesBackSpaceThroughXTest(h, "Medieval2", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit}))
        return 1;
    // fcitx5's XIM frontend marks a press as a repeat when the same key was released at the same time.
    if (!pressesBackSpaceThroughXTest(h, "wordcraft", "xim", fcitx::CapabilityFlags{}, fcitx::KeyStates(fcitx::KeyState::Repeat)))
        return 1;
    // Fields inside a web page have no autocompletion, and Google Sheets moves the text under a Shift+Left selection.
    if (!keepsForwarding(h, "microsoft-edge", "dbus", chromeCaps))
        return 1;
    // A selection in a page field, such as Google Sheets autocompletion, is reported instead of the caret.
    if (!keepsForwarding(h, "microsoft-edge", "dbus", chromeCaps, 239))
        return 1;
    // VTE terminals report no surrounding text either, but print Shift+Left as an escape sequence.
    if (!keepsForwarding(h, "gnome-terminal-server", "dbus", chromeCaps))
        return 1;
    if (!keepsForwarding(h, "xfce4-terminal", "dbus", chromeCaps))
        return 1;
    // XTEST would work for xterm too, but a DEL in the commit needs no wait.
    if (!erasesWithCommittedDel(h, "xterm", "xim", fcitx::CapabilityFlags{}))
        return 1;
    if (!keepsForwarding(h, "gtk3app", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix}))
        return 1;

    // WPS over IBus is only known to ignore forwarded keys under Xwayland.
    const auto wpsCaps = fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText};
    if (!keepsForwarding(h, "wps", "ibus", wpsCaps))
        return 1;

    // Under Xwayland XTEST asks the user first, so Wine gets key events sent to its window.
    unsetenv("DISPLAY");
    setXTestSenderForTest({});
    setXSendSenderForTest([&sent](int count) {
        sent.push_back(count);
        return true;
    });
    if (!pressesBackSpaceThroughXTest(h, "wine-preloader", "xim", fcitx::CapabilityFlags{}, fcitx::KeyStates(fcitx::KeyState::Repeat)))
        return 1;
    // WPS over IBus ignores forwarded keys and deletions through the surrounding text it claims to support.
    if (!pressesBackSpaceThroughXTest(h, "wps", "ibus", wpsCaps))
        return 1;
    if (!pressesBackSpaceThroughXTest(h, "et", "ibus", wpsCaps))
        return 1;
    if (!pressesBackSpaceThroughXTest(h, "wpp", "ibus", wpsCaps))
        return 1;
    if (!keepsForwarding(h, "qt5app", "ibus", wpsCaps))
        return 1;
    // With no sentinel coming, the word must not wait for one.
    setXSendSenderForTest([](int) { return false; });
    if (!commitsWhenBackspacesFail(h, "wps", "ibus", wpsCaps))
        return 1;
    setXSendSenderForTest([&sent](int count) {
        sent.push_back(count);
        return true;
    });
    // xterm drops sent events but runs a committed DEL like Backspace.
    if (!erasesWithCommittedDel(h, "xterm", "xim", fcitx::CapabilityFlags{}))
        return 1;
    if (!keepsForwarding(h, "wordcraft", "xim", fcitx::CapabilityFlags{}))
        return 1;
    if (!keepsForwarding(h, "gtk3app", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix}))
        return 1;
    setXSendSenderForTest({});
    return 0;
}
