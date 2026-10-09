// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-pointer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace {
    constexpr uint8_t XInputOpcode = 131;

    // Builds the first 32 bytes of an xcb_input_raw_button_press_event_t.
    void rawEvent(uint8_t (&event)[32], uint8_t responseType, uint8_t opcode, uint16_t type, uint32_t button) {
        std::memset(event, 0, sizeof(event));
        event[0] = responseType;
        event[1] = opcode;
        std::memcpy(event + 8, &type, sizeof(type));
        std::memcpy(event + 16, &button, sizeof(button));
    }

    bool expectClick(const std::string& step, uint8_t responseType, uint8_t opcode, uint16_t type, uint32_t button, bool expected) {
        uint8_t event[32];
        rawEvent(event, responseType, opcode, type, button);
        const bool actual = isRawClickEvent(event, XInputOpcode);
        if (actual == expected)
            return true;
        std::cerr << "Step: " << step << "\nExpected: " << (expected ? "click" : "no click") << "\nActual: " << (actual ? "click" : "no click") << '\n';
        return false;
    }

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

    // An X11 app on a Wayland desktop runs on Xwayland, which reports the clicks on its windows.
    bool watchesXwaylandClicks() {
        std::string display;
        const pid_t xvfb = startXvfb(display);
        if (xvfb < 0) {
            std::cerr << "Xvfb not found: skipping the Wayland session step\n";
            return true;
        }
        setenv("DISPLAY", display.c_str(), 1);
        setenv("WAYLAND_DISPLAY", "wayland-test", 1);
        std::atomic<bool> stop{false};
        bool              watched = false;
        std::thread       watcher([&] { watched = watchX11PointerClicks(stop, [] {}); });
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stop.store(true);
        watcher.join();
        kill(xvfb, SIGTERM);
        waitpid(xvfb, nullptr, 0);
        unsetenv("WAYLAND_DISPLAY");
        setenv("DISPLAY", "", 1);
        if (!watched)
            std::cerr << "Step: watch on a Wayland session with Xwayland\nExpected: returns true\nActual: returned false\n";
        return watched;
    }
} // namespace

int main() {
    bool ok = true;
    ok &= expectClick("left button press", 35, XInputOpcode, 15, 1, true);
    ok &= expectClick("right button press", 35, XInputOpcode, 15, 3, true);
    ok &= expectClick("side button press", 35, XInputOpcode, 15, 8, true);
    ok &= expectClick("press sent with SendEvent", 35 | 0x80, XInputOpcode, 15, 1, true);
    ok &= expectClick("wheel up", 35, XInputOpcode, 15, 4, false);
    ok &= expectClick("horizontal wheel", 35, XInputOpcode, 15, 7, false);
    ok &= expectClick("button release", 35, XInputOpcode, 16, 1, false);
    ok &= expectClick("event of another extension", 35, XInputOpcode + 1, 15, 1, false);
    ok &= expectClick("core key press", 2, XInputOpcode, 15, 1, false);

    // ctest runs with an empty DISPLAY: the watcher must give up at once so the server path takes over.
    std::atomic<bool> stop{false};
    int               clicks = 0;
    if (watchX11PointerClicks(stop, [&clicks] { ++clicks; })) {
        std::cerr << "Step: watch without an X11 session\nExpected: returns false\nActual: returned true\n";
        ok = false;
    }
    ok &= watchesXwaylandClicks();
    return ok ? 0 : 1;
}
