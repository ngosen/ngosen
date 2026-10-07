// SPDX-License-Identifier: GPL-3.0-or-later
//
// On an X11 session XTEST takes the uinput server's place. Clients whose existing path works keep it;
// Chromium-based clients, which report no surrounding text and whose address bar selects an inline
// autocompletion, select the old text with Shift+Left and type over it.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "ngosen-xtest.h"
#include "test-input-context.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

    // Stands in for the uinput server and counts the requests that reach it.
    class ServerProbe {
      public:
        ServerProbe() {
            fd_ = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
            sockaddr_un address{};
            address.sun_family    = AF_UNIX;
            const auto socketPath = buildSocketPath("kb_socket");
            address.sun_path[0]   = '\0';
            std::memcpy(&address.sun_path[1], socketPath.data(), socketPath.size());
            const auto length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + socketPath.size() + 1);
            if (fd_ < 0 || bind(fd_, reinterpret_cast<const sockaddr*>(&address), length) < 0 || listen(fd_, 4) < 0) {
                reportFailure("bind server socket", "bind succeeds", std::strerror(errno));
                if (fd_ >= 0)
                    close(fd_);
                fd_ = -1;
            }
        }
        ~ServerProbe() {
            for (int client : clients_)
                close(client);
            if (fd_ >= 0)
                close(fd_);
        }
        bool valid() const {
            return fd_ >= 0;
        }
        int requests() {
            for (int client = accept(fd_, nullptr, nullptr); client >= 0; client = accept(fd_, nullptr, nullptr))
                clients_.push_back(client);
            for (int client : clients_) {
                int    value = 0;
                pollfd p{client, POLLIN, 0};
                while (poll(&p, 1, 0) > 0 && recv(client, &value, sizeof(value), MSG_DONTWAIT) == sizeof(value))
                    ++count_;
            }
            return count_;
        }

      private:
        int              fd_    = -1;
        int              count_ = 0;
        std::vector<int> clients_;
    };

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
        ServerProbe&                      server;
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
        const int         before  = h.server.requests();
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
        if (h.server.requests() != before) {
            reportFailure(where + "nothing reaches the uinput server", "0", std::to_string(h.server.requests() - before));
            return false;
        }
        return true;
    }

    // SDL takes neither forwarded keys nor surrounding text; XTEST presses BackSpace like the server did,
    // and the last one comes back to the input method as the sentinel.
    bool sdlPressesBackSpaceThroughXTest(Harness& h) {
        const int before  = h.server.requests();
        auto      context = h.open("Medieval2", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit});
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
        if (h.server.requests() != before) {
            reportFailure("SDL: nothing reaches the uinput server", "0", std::to_string(h.server.requests() - before));
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
    const std::string socketNamespace = "test-" + std::to_string(getpid());
    setenv("LOTUS_SOCKET_NAMESPACE", socketNamespace.c_str(), 1);
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
    config.setValueByPath("Mode", "Uinput");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    engine.setConfig(config);

    ServerProbe server;
    if (!server.valid())
        return 1;
    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
    Harness                 h{testInstance, engine, entry, server, sent};

    const auto              chromeCaps = fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit, fcitx::CapabilityFlag::KeyEventOrderFix};
    if (!chromiumSelectsAndOvertypes(h, "microsoft-edge", "dbus", chromeCaps))
        return 1;
    if (!chromiumSelectsAndOvertypes(h, "microsoft-edge", "dbus", chromeCaps, true))
        return 1;
    if (!chromiumSelectsAndOvertypes(h, "chromium", "fcitx4", fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::FormattedPreedit}))
        return 1;
    if (!sdlPressesBackSpaceThroughXTest(h))
        return 1;
    if (!keepsForwarding(h, "gtk3app", "dbus", fcitx::CapabilityFlags{fcitx::CapabilityFlag::SurroundingText, fcitx::CapabilityFlag::KeyEventOrderFix}))
        return 1;
    if (!keepsForwarding(h, "geany", "xim", fcitx::CapabilityFlags{}))
        return 1;
    setXTestSenderForTest({});
    return 0;
}
