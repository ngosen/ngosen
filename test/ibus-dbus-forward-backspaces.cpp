// SPDX-License-Identifier: GPL-3.0-or-later
//
// On the ibus, dbus and fcitx4 frontends the backspaces of a replacement go to the app through
// forwardKey, or through deleteSurroundingText for GTK4 clients, which drop forwarded keys. SDL clients
// take neither, so they get nothing; on X11 they get XTEST keys (x11-xtest-replacement.cpp).
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <cstddef>
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
        TestInstance&                     testInstance;
        fcitx::LotusEngine&               engine;
        fcitx::InputMethodEntry&          entry;

        std::unique_ptr<TestInputContext> open(const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
            auto context = std::make_unique<TestInputContext>(&testInstance.instance, program, frontend);
            context->setCapabilityFlags(caps);
            context->focusIn();
            fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, focus);
            return context;
        }

        bool type(TestInputContext& context, fcitx::KeySym symbol, bool accepted) {
            fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
            engine.keyEvent(entry, event);
            if (event.accepted() != accepted) {
                reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(accepted), "accepted=" + std::to_string(event.accepted()));
                return false;
            }
            return true;
        }

        // Types "tie" and reports each step to the context when it declares surrounding text.
        bool typeTie(TestInputContext& context) {
            const bool        surrounding = context.capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText);
            const std::string word        = "tie";
            for (size_t i = 0; i < word.size(); ++i) {
                if (!type(context, static_cast<fcitx::KeySym>(word[i]), false))
                    return false;
                if (surrounding)
                    setSnapshot(context, word.substr(0, i + 1), static_cast<unsigned int>(i + 1));
            }
            return true;
        }

        static void setSnapshot(TestInputContext& context, const std::string& text, unsigned int cursor) {
            context.surroundingText().setText(text, cursor, cursor);
            context.updateSurroundingText();
        }

        template <typename Done>
        uint64_t pumpUntil(Done done, uint64_t deadlineUs) {
            while (!done() && nowUs() < deadlineUs)
                pumpEventLoop(testInstance.instance, 1);
            return nowUs();
        }
    };

    bool forwardedOneBackspace(const TestInputContext& context) {
        const auto& forwarded = context.forwarded();
        return forwarded.size() == 2 && forwarded[0].key().sym() == FcitxKey_BackSpace && !forwarded[0].isRelease() && forwarded[1].key().sym() == FcitxKey_BackSpace &&
            forwarded[1].isRelease();
    }

    // fcitx5-gtk (GTK3) and fcitx5-qt keep forwarded keys in order with commits.
    bool dbusForwardsAndWaitsForDeletion(Harness& h) {
        const auto caps    = fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix};
        auto       context = h.open("gtk3app", "dbus", caps);
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, true))
            return false;
        if (!forwardedOneBackspace(*context)) {
            reportFailure("dbus: forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }
        pumpEventLoop(h.testInstance.instance, 3);
        if (!context->commits().empty()) {
            reportFailure("dbus: no commit before the app reports the deletion", "(none)", joinCommits(*context));
            return false;
        }
        Harness::setSnapshot(*context, "ti", 2);
        pumpEventLoop(h.testInstance.instance, 120);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("dbus: commit once the app reports the deletion", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // GTK4 clients drop forwarded keys, so the replacement deletes through surrounding text.
    bool gtk4DeletesThroughSurroundingText(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        auto context = h.open(program, frontend, caps);
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, true))
            return false;
        pumpEventLoop(h.testInstance.instance, 20);
        const std::string where = frontend + " " + program + ": ";
        if (!context->forwarded().empty()) {
            reportFailure(where + "no forwarded key", "(none)", describeForwarded(*context));
            return false;
        }
        if (context->deletes() != std::vector<std::pair<int, unsigned int>>{{-1, 1}}) {
            reportFailure(where + "delete the e through surrounding text", "one delete (-1, 1)", std::to_string(context->deletes().size()) + " deletes");
            return false;
        }
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(where + "commit after the delete", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // Firefox over IBus and Chromium over the fcitx4 GTK module report no surrounding text; their
    // forwarded keys queue behind the commit unless the commit waits.
    bool forwardWithoutSurroundingWaits(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        auto              context = h.open(program, frontend, caps);
        const std::string where   = frontend + " " + program + ": ";
        if (!h.typeTie(*context))
            return false;
        // libuv dates a new timer from the loop's last run, and a real key arrives inside one.
        pumpEventLoop(h.testInstance.instance, 1);
        const uint64_t replacedAt = nowUs();
        if (!h.type(*context, FcitxKey_e, true))
            return false;
        if (!forwardedOneBackspace(*context)) {
            reportFailure(where + "forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }
        const uint64_t committedAt = h.pumpUntil([&] { return !context->commits().empty(); }, replacedAt + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(where + "commit e -> ê", "['ê']", joinCommits(*context));
            return false;
        }
        if (committedAt - replacedAt + ClockSlackUs < WaitUs) {
            reportFailure(where + "commit at least 15 ms after the forward", ">= 15000 us", std::to_string(committedAt - replacedAt) + " us");
            return false;
        }
        return true;
    }

    // Snap apps load the fcitx4 GTK module, which keeps forwarded keys ahead of commits it reports
    // surrounding text for.
    bool fcitx4ForwardsWithSurroundingText(Harness& h) {
        const auto caps    = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::SurroundingText};
        auto       context = h.open("firefox", "fcitx4", caps);
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, true))
            return false;
        if (!forwardedOneBackspace(*context)) {
            reportFailure("fcitx4 firefox: forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }
        Harness::setSnapshot(*context, "ti", 2);
        const uint64_t start = nowUs();
        h.pumpUntil([&] { return !context->commits().empty(); }, start + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("fcitx4 firefox: commit e -> ê", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // SDL handles only commits and preedit from the IM, so neither forwarded keys nor surrounding text
    // deletes anything there. Without XTEST nothing can delete, and the key goes through unchanged.
    bool sdlGetsNoDeletion(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
        auto context = h.open(program, frontend, caps);
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, false))
            return false;
        pumpEventLoop(h.testInstance.instance, 20);
        const std::string where = frontend + " " + program + ": ";
        if (!context->forwarded().empty() || !context->deletes().empty()) {
            reportFailure(where + "no forwarded key or surrounding delete", "(none)", describeForwarded(*context) + ", " + std::to_string(context->deletes().size()) + " deletes");
            return false;
        }
        return true;
    }

    // GNOME Shell is the IBus client there and forwards the keys to the app like any other client.
    bool gnomeIbusForwards(Harness& h) {
        setenv("XDG_CURRENT_DESKTOP", "GNOME", 1);
        auto context = h.open("gnome-shell", "ibus", fcitx::CapabilityFlags{});
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, true))
            return false;
        if (!forwardedOneBackspace(*context)) {
            reportFailure("GNOME ibus: forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }
        pumpEventLoop(h.testInstance.instance, 40);
        return true;
    }

} // namespace

int main() {
    setenv("XDG_CURRENT_DESKTOP", "KDE", 1);

    configureTestPaths("fcitx5-lotus-ibus-dbus-forward-backspaces");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    engine.setConfig(config);

    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
    Harness                 h{testInstance, engine, entry};

    if (!dbusForwardsAndWaitsForDeletion(h))
        return 1;
    if (!gtk4DeletesThroughSurroundingText(h, "gtk4app", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText}))
        return 1;
    if (!gtk4DeletesThroughSurroundingText(h, "gtk4-im:gtk4app", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText}))
        return 1;
    if (!forwardWithoutSurroundingWaits(h, "gtk3-im:firefox", "ibus", fcitx::CapabilityFlags{}))
        return 1;
    if (!forwardWithoutSurroundingWaits(h, "chrome", "fcitx4", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit}))
        return 1;
    if (!fcitx4ForwardsWithSurroundingText(h))
        return 1;
    const auto sdlCaps = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit};
    if (!sdlGetsNoDeletion(h, "Medieval2", "dbus", sdlCaps))
        return 1;
    if (!sdlGetsNoDeletion(h, "SDL2_Application", "ibus", fcitx::CapabilityFlags{}))
        return 1;
    // SDL 2.0.12 and older speak the fcitx4 protocol and set at most Preedit.
    if (!sdlGetsNoDeletion(h, "Medieval2", "fcitx4", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit}))
        return 1;
    if (!gnomeIbusForwards(h))
        return 1;
    return 0;
}
