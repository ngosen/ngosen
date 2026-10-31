// SPDX-License-Identifier: GPL-3.0-or-later
//
// On the ibus, dbus and fcitx4 frontends the backspaces of a replacement go to the app through
// forwardKey, or through deleteSurroundingText for GTK4 clients, which drop forwarded keys. SDL clients
// take neither, so they get nothing; on X11 they get XTEST keys (x11-xtest-replacement.cpp).
#include "ngosen-engine.h"
#include "ngosen-utils.h"
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
    constexpr uint64_t ShellWaitUs  = 40000;
    constexpr uint64_t SettleUs     = 40000;

    struct Harness {
        TestInstance&                     testInstance;
        fcitx::NgoSenEngine&              engine;
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

        // Types "tie" and reports each step to the context when it declares surrounding text. A lagging
        // field reports each step one key late, so its copy misses the last letter.
        bool typeTie(TestInputContext& context, bool lagging = false, bool reportsWord = true) {
            const bool        surrounding = reportsWord && context.capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText);
            const std::string word        = "tie";
            for (size_t i = 0; i < word.size(); ++i) {
                if (!type(context, static_cast<fcitx::KeySym>(word[i]), false))
                    return false;
                const size_t reported = lagging ? i : i + 1;
                if (surrounding && reported > 0)
                    setSnapshot(context, word.substr(0, reported), static_cast<unsigned int>(reported));
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

    // fcitx5-gtk (GTK3) and fcitx5-qt delete through the surrounding text once the field has reported
    // the word.
    bool dbusDeletesReportedWord(Harness& h) {
        const auto caps    = fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix};
        auto       context = h.open("gtk3app", "dbus", caps);
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, true))
            return false;
        const bool deleted = context->deletes() == std::vector<std::pair<int, unsigned int>>{{-1, 1}};
        if (!deleted || !context->forwarded().empty() || context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("dbus: e -> ê through the surrounding text", "deletes=1 forwarded=(none) ['ê']",
                          "deletes=" + std::to_string(context->deletes().size()) + " forwarded=" + describeForwarded(*context) + " " + joinCommits(*context));
            return false;
        }
        return true;
    }

    // Firefox over D-Bus reports the word but stays on backspaces (it reports its own edits late), so
    // the forwarded keys keep their order with the commit, which waits for the report of the deletion.
    bool dbusForwardsAndWaitsForDeletion(Harness& h) {
        const auto caps    = fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix};
        auto       context = h.open("firefox", "dbus", caps);
        if (!h.typeTie(*context) || !h.type(*context, FcitxKey_e, true))
            return false;
        if (!forwardedOneBackspace(*context)) {
            reportFailure("dbus firefox: forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }
        pumpEventLoop(h.testInstance.instance, 3);
        if (!context->commits().empty()) {
            reportFailure("dbus firefox: no commit before the app reports the deletion", "(none)", joinCommits(*context));
            return false;
        }
        Harness::setSnapshot(*context, "ti", 2);
        pumpEventLoop(h.testInstance.instance, 120);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("dbus firefox: commit once the app reports the deletion", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // Before the field reports any text no report can confirm the deletion, so the commit follows the
    // forwarded backspace on a timer. The key's own turn must not commit; the loop then may at any time.
    bool dbusCommitsAfterForwardBeforeFirstReport(Harness& h) {
        const auto caps    = fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix};
        auto       context = h.open("gtk3app", "dbus", caps);
        if (!h.typeTie(*context, false, false) || !h.type(*context, FcitxKey_e, true))
            return false;
        if (!forwardedOneBackspace(*context)) {
            reportFailure("dbus unreported: forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }
        if (!context->commits().empty()) {
            reportFailure("dbus unreported: no commit in the key's own turn", "(none)", joinCommits(*context));
            return false;
        }
        pumpEventLoop(h.testInstance.instance, 120);
        if (context->commits() != std::vector<std::string>{"ê"} || context->deletes().size() != 0) {
            reportFailure("dbus unreported: commit after the timer, no surrounding deletion", "['ê'] deletes=0",
                          joinCommits(*context) + " deletes=" + std::to_string(context->deletes().size()));
            return false;
        }
        return true;
    }

    // fcitx5-gtk (GTK3) gets the commit after the reply to the key event, and puts back a key we let
    // through on its own queue, so the two race: typing "roofi" fast in gnome-terminal showed "riồ".
    // A key typed during the wait must go out inside the commit.
    bool dbusKeyDuringWaitJoinsCommit(Harness& h) {
        auto context = h.open("gnome-terminal-server", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::KeyEventOrderFix});
        if (!h.type(*context, FcitxKey_r, false) || !h.type(*context, FcitxKey_o, false))
            return false;
        pumpEventLoop(h.testInstance.instance, 1);
        if (!h.type(*context, FcitxKey_o, true))
            return false;
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 500000);
        pumpEventLoop(h.testInstance.instance, 1);
        if (!h.type(*context, FcitxKey_f, true) || !h.type(*context, FcitxKey_i, true))
            return false;
        h.pumpUntil([&] { return context->commits().size() > 1; }, nowUs() + 500000);
        if (context->commits() != std::vector<std::string>{"ô", "ồi"}) {
            reportFailure("dbus: a key typed during the wait goes out with the replacement", "['ô']['ồi']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // GTK4 clients drop forwarded keys, and through GNOME's IBus bridge forwarded keys can land after
    // the commit (#362), so these replacements delete through surrounding text.
    bool deletesThroughSurroundingText(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps) {
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

    // GNOME Shell passes the forwarded keys on to the app and its reports back to us one edit at a time,
    // sometimes later than any fixed wait. The commit waits for the report that shows the deletion.
    bool gnomeIbusWaitsForReportedDeletion(Harness& h) {
        auto context = h.open("window:1", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText});
        if (!h.typeTie(*context, true))
            return false;
        pumpEventLoop(h.testInstance.instance, 1);
        if (!h.type(*context, FcitxKey_e, true))
            return false;
        Harness::setSnapshot(*context, "tie", 3);
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 30000);
        if (!context->commits().empty()) {
            reportFailure("GNOME ibus: no commit before the app reports the deletion", "(none)", joinCommits(*context));
            return false;
        }
        Harness::setSnapshot(*context, "ti", 2);
        h.pumpUntil([&] { return !context->commits().empty(); }, nowUs() + 10000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("GNOME ibus: commit once the app reports the deletion", "['ê']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // A report can arrive from before the forwarded keys were applied, so a commit never goes out
    // sooner than the fixed wait would have sent it.
    bool gnomeIbusCommitsNoSoonerThanForwardWait(Harness& h) {
        auto context = h.open("window:3", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText});
        if (!h.typeTie(*context, true))
            return false;
        pumpEventLoop(h.testInstance.instance, 1);
        const uint64_t replacedAt = nowUs();
        if (!h.type(*context, FcitxKey_e, true))
            return false;
        Harness::setSnapshot(*context, "ti", 2);
        const uint64_t committedAt = h.pumpUntil([&] { return !context->commits().empty(); }, replacedAt + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("GNOME ibus early report: commit e -> ê", "['ê']", joinCommits(*context));
            return false;
        }
        if (committedAt - replacedAt + ClockSlackUs < ShellWaitUs) {
            reportFailure("GNOME ibus early report: commit at least 40 ms after the forward", ">= 40000 us", std::to_string(committedAt - replacedAt) + " us");
            return false;
        }
        return true;
    }

    // A chat composer drops a commit that lands right after the deletion (#362), so an early report
    // there still waits out the composer settle, not just the fixed wait.
    bool gnomeIbusComposerWaitsForSettle(Harness& h) {
        auto context = h.open("window:4", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText});
        Harness::setSnapshot(*context, "ok \n\n", 3);
        const std::string word = "tie";
        for (size_t i = 0; i < word.size(); ++i) {
            if (!h.type(*context, static_cast<fcitx::KeySym>(word[i]), false))
                return false;
            // Reported one key late, so the copy misses the last letter.
            Harness::setSnapshot(*context, "ok " + word.substr(0, i) + "\n\n", static_cast<unsigned int>(i + 3));
        }
        pumpEventLoop(h.testInstance.instance, 1);
        const uint64_t replacedAt = nowUs();
        if (!h.type(*context, FcitxKey_e, true))
            return false;
        Harness::setSnapshot(*context, "ok ti\n\n", 5);
        const uint64_t committedAt = h.pumpUntil([&] { return !context->commits().empty(); }, replacedAt + 500000);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure("GNOME ibus composer: commit e -> ê", "['ê']", joinCommits(*context));
            return false;
        }
        if (committedAt - replacedAt + ClockSlackUs < SettleUs) {
            reportFailure("GNOME ibus composer: commit at least 40 ms after an early report", ">= 40000 us", std::to_string(committedAt - replacedAt) + " us");
            return false;
        }
        h.type(*context, FcitxKey_space, false);
        return true;
    }

    // GNOME Shell 46 declares surrounding text for every app, so a terminal that never reports any
    // would wait out every timeout. After two silent waits the fixed wait takes over until a report.
    bool gnomeIbusStopsWaitingForSilentApp(Harness& h) {
        auto context = h.open("window:2", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText});
        Harness::setSnapshot(*context, "ls -l", 5);
        // Types `word`, then `key` that changes it; returns how long the commit took.
        auto replace = [&](const std::string& word, char key) -> uint64_t {
            for (const char c : word)
                if (!h.type(*context, static_cast<fcitx::KeySym>(c), false))
                    return 0;
            pumpEventLoop(h.testInstance.instance, 1);
            const size_t   before     = context->commits().size();
            const uint64_t replacedAt = nowUs();
            if (!h.type(*context, static_cast<fcitx::KeySym>(key), true))
                return 0;
            const uint64_t waited = h.pumpUntil([&] { return context->commits().size() > before; }, replacedAt + 500000) - replacedAt;
            h.type(*context, FcitxKey_space, false);
            return waited;
        };
        replace("tien", 'e');
        replace("tien", 'e');
        const uint64_t oneKeyUs = replace("to", 'o');
        if (oneKeyUs + ClockSlackUs < ShellWaitUs || oneKeyUs > 55000) {
            reportFailure("GNOME ibus silent app: fixed wait for one backspace", "40000-55000 us", std::to_string(oneKeyUs) + " us");
            return false;
        }
        // Any report means the app does report, so the next replacement waits for it again.
        Harness::setSnapshot(*context, "ls -l tô ", 9);
        const uint64_t reportedUs = replace("tien", 'e');
        if (reportedUs < 60000) {
            reportFailure("GNOME ibus silent app: wait for a report once the app sends one", ">= 60000 us", std::to_string(reportedUs) + " us");
            return false;
        }
        if (context->commits() != std::vector<std::string>{"ên", "ên", "ô", "ên"}) {
            reportFailure("GNOME ibus silent app: commit each replacement", "['ên']['ên']['ô']['ên']", joinCommits(*context));
            return false;
        }
        return true;
    }

    // Firefox over IBus and Chromium over the fcitx4 GTK module report no surrounding text; their
    // forwarded keys queue behind the commit unless the commit waits.
    bool forwardWithoutSurroundingWaits(Harness& h, const std::string& program, const std::string& frontend, fcitx::CapabilityFlags caps, uint64_t waitUs) {
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
        if (committedAt - replacedAt + ClockSlackUs < waitUs) {
            reportFailure(where + "commit no sooner than the forward wait", ">= " + std::to_string(waitUs) + " us", std::to_string(committedAt - replacedAt) + " us");
            return false;
        }
        // Other frontends keep the short wait; the GNOME one costs 25 ms per replacement.
        if (committedAt - replacedAt > waitUs + 15000) {
            reportFailure(where + "commit soon after the forward wait", "<= " + std::to_string(waitUs + 15000) + " us", std::to_string(committedAt - replacedAt) + " us");
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

    configureTestPaths("fcitx5-ngosen-ibus-dbus-forward-backspaces");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    // Longer than the GNOME forward wait, so waiting for a report stays distinguishable from the fixed wait.
    config.setValueByPath("WaitSurroundingShortMs", "70");
    engine.setConfig(config);

    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    Harness                 h{testInstance, engine, entry};

    if (!dbusDeletesReportedWord(h) || !dbusForwardsAndWaitsForDeletion(h) || !dbusCommitsAfterForwardBeforeFirstReport(h))
        return 1;
    if (!dbusKeyDuringWaitJoinsCommit(h))
        return 1;
    if (!deletesThroughSurroundingText(h, "gtk4app", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText}))
        return 1;
    if (!deletesThroughSurroundingText(h, "gtk4-im:gtk4app", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText}))
        return 1;
    if (!deletesThroughSurroundingText(h, "window:5", "ibus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText}))
        return 1;
    if (!gnomeIbusWaitsForReportedDeletion(h))
        return 1;
    if (!gnomeIbusCommitsNoSoonerThanForwardWait(h))
        return 1;
    if (!gnomeIbusComposerWaitsForSettle(h))
        return 1;
    if (!gnomeIbusStopsWaitingForSilentApp(h))
        return 1;
    if (!forwardWithoutSurroundingWaits(h, "gtk3-im:firefox", "ibus", fcitx::CapabilityFlags{}, ShellWaitUs))
        return 1;
    if (!forwardWithoutSurroundingWaits(h, "chrome", "fcitx4", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit}, WaitUs))
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
