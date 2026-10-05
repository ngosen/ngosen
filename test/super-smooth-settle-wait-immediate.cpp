// SPDX-License-Identifier: GPL-3.0-or-later
//
// Messenger repaints its composer a few ms after Edge reports the deletion as done, wiping a commit
// that lands before the repaint. WaitSurroundingSettleMs holds the commit that long.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

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

    class BackspaceListener {
      public:
        BackspaceListener() {
            fd_ = socket(AF_UNIX, SOCK_SEQPACKET, 0);
            sockaddr_un address{};
            address.sun_family    = AF_UNIX;
            const auto socketPath = buildSocketPath("kb_socket");
            address.sun_path[0]   = '\0';
            std::memcpy(&address.sun_path[1], socketPath.data(), socketPath.size());
            const auto length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + socketPath.size() + 1);
            if (fd_ < 0 || bind(fd_, reinterpret_cast<const sockaddr*>(&address), length) < 0 || listen(fd_, 1) < 0) {
                reportFailure("bind replacement socket", "bind succeeds", std::strerror(errno));
                if (fd_ >= 0)
                    close(fd_);
                fd_ = -1;
            }
        }
        ~BackspaceListener() {
            if (client_ >= 0)
                close(client_);
            if (fd_ >= 0)
                close(fd_);
        }
        bool valid() const {
            return fd_ >= 0;
        }
        bool receive(int& count) {
            if (client_ < 0) {
                pollfd p{fd_, POLLIN, 0};
                if (poll(&p, 1, 2000) <= 0 || (client_ = accept(fd_, nullptr, nullptr)) < 0) {
                    reportFailure("accept replacement socket", "connection within 2000 ms", "none");
                    return false;
                }
            }
            pollfd p{client_, POLLIN, 0};
            if (poll(&p, 1, 2000) <= 0 || recv(client_, &count, sizeof(count), 0) != sizeof(count)) {
                reportFailure("receive replacement request", "request within 2000 ms", "none");
                return false;
            }
            return true;
        }

      private:
        int fd_     = -1;
        int client_ = -1;
    };

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
    const std::string socketNamespace = "test-" + std::to_string(getpid());
    setenv("LOTUS_SOCKET_NAMESPACE", socketNamespace.c_str(), 1);

    configureTestPaths("fcitx5-lotus-super-smooth-settle-wait-immediate");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    // Covers the delete-then-commit fallback, not the default select-and-overtype path.
    config.setValueByPath("MessengerSelectOvertype", "False");
    // Long enough that a stalled machine cannot let the wait expire before the "no commit" check.
    config.setValueByPath("WaitSurroundingSettleMs", "500");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::LotusMode::Uinput || !engine.config().waitSurroundingEvent.value()) {
        reportFailure("configure Uinput", "mode=Uinput, WaitSurroundingEvent=True", "config differs");
        return 1;
    }

    BackspaceListener listener;
    if (!listener.valid())
        return 1;
    auto context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    // Plain letters reach the page as real keys; the page reports each one.
    setSnapshot(*context, "\n\n", 0);
    const std::string word = "tie";
    for (size_t i = 0; i < word.size(); ++i) {
        if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(word[i]), false))
            return 1;
        setSnapshot(*context, word.substr(0, i + 1) + "\n\n", static_cast<unsigned int>(i + 1));
    }

    // Telex "e" again: e -> ê, one character to delete plus the trigger backspace.
    if (!type(engine, entry, *context, FcitxKey_e, true))
        return 1;
    int backspaces = 0;
    if (!listener.receive(backspaces))
        return 1;
    if (backspaces != 2) {
        reportFailure("backspace count for e -> ê", "2", std::to_string(backspaces));
        return 1;
    }
    // The page already reports the deletion done when the trigger backspace arrives
    // (the "ngay" branch); the settle wait must still hold the commit.
    for (int i = 0; i < backspaces; ++i) {
        if (i + 1 == backspaces)
            setSnapshot(*context, "ti\n\n", 2);
        if (!type(engine, entry, *context, FcitxKey_BackSpace, i + 1 == backspaces))
            return 1;
    }
    pumpEventLoop(testInstance.instance, 15);
    if (!context->commits().empty()) {
        reportFailure("no commit while the settle wait runs, after the snapshot shows the deletion done", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    pumpEventLoop(testInstance.instance, 600);
    if (context->commits() != std::vector<std::string>{"ê"}) {
        reportFailure("commit once the settle wait is over", "commits=['ê']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
