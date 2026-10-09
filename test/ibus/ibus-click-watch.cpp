/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// A click on an X11 window ends the word being typed, as it does in the fcitx5 version.
#include "ngosen-globals.h"
#include "ngosen-ibus-clicks.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <syslog.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace {

    // Starts Xvfb on a free display and returns its pid, or -1 when Xvfb is missing.
    pid_t startXvfb(std::string& display) {
        int fds[2];
        if (pipe(fds) != 0)
            return -1;
        const std::string displayFd = std::to_string(fds[1]);
        const char*       argv[]    = {"Xvfb", "-displayfd", displayFd.c_str(), "-nolisten", "tcp", nullptr};
        pid_t             pid       = -1;
        if (posix_spawnp(&pid, "Xvfb", nullptr, nullptr, const_cast<char* const*>(argv), environ) != 0)
            pid = -1;
        close(fds[1]);
        char    buffer[16] = {};
        ssize_t n          = pid > 0 ? read(fds[0], buffer, sizeof(buffer) - 1) : 0;
        close(fds[0]);
        if (n <= 0) {
            if (pid > 0) {
                kill(pid, SIGTERM);
                waitpid(pid, nullptr, 0);
            }
            return -1;
        }
        display = ":" + std::string(buffer, buffer[n - 1] == '\n' ? n - 1 : n);
        return pid;
    }

    struct XcbConnection;
    struct XcbCookie {
        unsigned int sequence;
    };
    using ConnectFn    = XcbConnection* (*)(const char*, int*);
    using DisconnectFn = void (*)(XcbConnection*);
    using CheckFn      = void* (*)(XcbConnection*, XcbCookie);
    using FakeInputFn  = XcbCookie (*)(XcbConnection*, uint8_t, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint8_t);

    // Clicks the left button through XTEST; false when XTEST is missing or refuses.
    bool fakeClick() {
        void* xcb   = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
        void* xtest = dlopen("libxcb-xtest.so.0", RTLD_NOW | RTLD_LOCAL);
        if (xcb == nullptr || xtest == nullptr)
            return false;
        auto connect    = reinterpret_cast<ConnectFn>(dlsym(xcb, "xcb_connect"));
        auto disconnect = reinterpret_cast<DisconnectFn>(dlsym(xcb, "xcb_disconnect"));
        auto check      = reinterpret_cast<CheckFn>(dlsym(xcb, "xcb_request_check"));
        auto fake       = reinterpret_cast<FakeInputFn>(dlsym(xtest, "xcb_test_fake_input_checked"));
        if (connect == nullptr || disconnect == nullptr || check == nullptr || fake == nullptr)
            return false;
        XcbConnection*    conn          = connect(nullptr, nullptr);
        constexpr uint8_t ButtonPress   = 4;
        constexpr uint8_t ButtonRelease = 5;
        bool              sent          = true;
        for (const uint8_t type : {ButtonPress, ButtonRelease}) {
            void* error = check(conn, fake(conn, type, 1, 0, 0, 0, 0, 0));
            sent &= error == nullptr;
            std::free(error);
        }
        disconnect(conn);
        return sent;
    }

    bool waitForClick() {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < end) {
            if (g_mouse_clicked.load())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

} // namespace

int main() {
    // The engine logs to syslog; show its lines here when the test fails.
    openlog("ibus_click_watch", LOG_PERROR, LOG_USER);
    std::string display;
    const pid_t xvfb = startXvfb(display);
    if (xvfb < 0) {
        std::cout << "Xvfb not found: skipping\n";
        return 0;
    }
    setenv("DISPLAY", display.c_str(), 1);
    setenv("WAYLAND_DISPLAY", "wayland-test", 1);
    bool ok = true;
    {
        ngosen::ClickWatch watch;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (!fakeClick()) {
            std::cout << "XTEST unavailable: skipping\n";
        } else if (!waitForClick() || !needEngineReset.load()) {
            std::cerr << "Step: left click on the X server while the engine runs\nExpected: the word being typed ends\nActual: no click seen\n";
            ok = false;
        }
    }
    kill(xvfb, SIGTERM);
    waitpid(xvfb, nullptr, 0);
    return ok ? 0 : 1;
}
