// SPDX-License-Identifier: GPL-3.0-or-later
//
// Keys typed while a replacement is pending are replayed after it. Under GNOME, mutter sends one
// text-input "done" per main-loop turn and the client keeps only the last commit_string before it, so
// committing the replacement and the replayed keys separately loses the replacement: typing "ddi"
// fast in the Facebook composer showed "i" instead of "đi". They must go out as one commit.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

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

    class RequestListener {
      public:
        RequestListener() {
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
        ~RequestListener() {
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

    configureTestPaths("fcitx5-lotus-replay-single-commit");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    config.setValueByPath("MessengerSelectOvertype", "True");
    engine.setConfig(config);

    RequestListener listener;
    if (!listener.valid())
        return 1;
    auto context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    // The Facebook composer: empty text followed by the two newlines the page keeps after the cursor.
    setSnapshot(*context, "\n\n", 0);
    if (!type(engine, entry, *context, FcitxKey_d, false))
        return 1;
    setSnapshot(*context, "d\n\n", 1);

    // Telex "dd": d -> đ, selected and typed over.
    if (!type(engine, entry, *context, FcitxKey_d, true))
        return 1;
    int request = 0;
    if (!listener.receive(request) || request != -1) {
        reportFailure("ask the server to select one character", "-1", std::to_string(request));
        return 1;
    }
    // "i" arrives before the field confirms the selection, so it is queued.
    if (!type(engine, entry, *context, FcitxKey_i, true))
        return 1;
    if (!context->commits().empty()) {
        reportFailure("nothing committed before the selection is confirmed", "commits=(none)", "commits=" + joinCommits(*context));
        return 1;
    }

    context->surroundingText().setText("d\n\n", 1, 0);
    context->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 10);
    if (context->commits() != std::vector<std::string>{"đi"}) {
        reportFailure("replacement and replayed key go out as one commit", "commits=['đi']", "commits=" + joinCommits(*context));
        return 1;
    }
    return 0;
}
