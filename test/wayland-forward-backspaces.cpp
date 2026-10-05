// SPDX-License-Identifier: GPL-3.0-or-later
//
// On the Wayland frontend the backspaces of a replacement go to the app through forwardKey instead of
// the uinput server, and the commit still waits until the app reports the deletion done.
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

    // Stands in for the uinput server and records whether any request reached it.
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
        // Number of replacement requests received so far; connecting alone is not a request.
        int requests() {
            for (int client = accept(fd_, nullptr, nullptr); client >= 0; client = accept(fd_, nullptr, nullptr))
                clients_.push_back(client);
            int count = 0;
            for (int client : clients_) {
                int    value = 0;
                pollfd p{client, POLLIN, 0};
                while (poll(&p, 1, 0) > 0 && recv(client, &value, sizeof(value), MSG_DONTWAIT) == sizeof(value))
                    ++count;
            }
            return count;
        }

      private:
        int              fd_ = -1;
        std::vector<int> clients_;
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

    std::string describeForwarded(const TestInputContext& context) {
        std::string out;
        for (const auto& event : context.forwarded())
            out += "[" + event.key().toString() + (event.isRelease() ? " up" : " down") + "]";
        return out.empty() ? "(none)" : out;
    }

} // namespace

int main() {
    const std::string socketNamespace = "test-" + std::to_string(getpid());
    setenv("LOTUS_SOCKET_NAMESPACE", socketNamespace.c_str(), 1);

    configureTestPaths("fcitx5-lotus-wayland-forward-backspaces");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    // Left at its default on purpose: the "\n\n" snapshot below looks like a Facebook composer, whose
    // Shift+Left overtype needs the uinput server and must not run on this frontend.
    config.setValueByPath("MessengerSelectOvertype", "True");
    engine.setConfig(config);

    ServerProbe server;
    if (!server.valid())
        return 1;
    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "test", "wayland");
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    setSnapshot(*context, "\n\n", 0);
    const std::string word = "tie";
    for (size_t i = 0; i < word.size(); ++i) {
        if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(word[i]), false))
            return 1;
        setSnapshot(*context, word.substr(0, i + 1) + "\n\n", static_cast<unsigned int>(i + 1));
    }

    // Telex "e" again: e -> ê deletes one character.
    if (!type(engine, entry, *context, FcitxKey_e, true))
        return 1;
    const auto& forwarded = context->forwarded();
    if (forwarded.size() != 2 || forwarded[0].key().sym() != FcitxKey_BackSpace || forwarded[0].isRelease() || forwarded[1].key().sym() != FcitxKey_BackSpace ||
        !forwarded[1].isRelease()) {
        reportFailure("forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
        return 1;
    }

    // The app has not reported the deletion yet, so nothing may be committed.
    pumpEventLoop(testInstance.instance, 3);
    if (!context->commits().empty()) {
        reportFailure("no commit before the app reports the deletion", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    setSnapshot(*context, "ti\n\n", 2);
    pumpEventLoop(testInstance.instance, 120);
    if (context->commits() != std::vector<std::string>{"ê"}) {
        reportFailure("commit once the app reports the deletion", "commits=['ê']", "commits=" + joinCommits(*context));
        return 1;
    }

    if (const int requests = server.requests(); requests != 0) {
        reportFailure("no request reaches the uinput server", "0", std::to_string(requests));
        return 1;
    }
    return 0;
}
