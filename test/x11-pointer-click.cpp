// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-pointer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <optional>
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
        const bool actual = ngosen::isRawClickEvent(event, XInputOpcode);
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
        std::thread       watcher([&] { watched = ngosen::watchX11PointerClicks(stop, [] {}); });
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
    bool expectSelection(const std::string& step, const std::optional<std::string>& actual, const std::optional<std::string>& expected) {
        if (actual == expected)
            return true;
        std::cerr << "Step: " << step << "\nExpected: " << expected.value_or("(none)") << "\nActual: " << actual.value_or("(none)") << '\n';
        return false;
    }

    bool keepsSelection() {
        bool                 ok = true;
        ngosen::X11Selection selection;
        selection.arrive(selection.announce(), "nhanh");
        ok &= expectSelection("a key after a selection", selection.take(), "nhanh");
        ok &= expectSelection("the next key", selection.take(), std::nullopt);

        uint64_t ticket = selection.announce();
        selection.forget();
        selection.arrive(ticket, "nhanh");
        ok &= expectSelection("a click before the text arrived", selection.take(), std::nullopt);

        ticket = selection.announce();
        selection.take();
        selection.arrive(ticket, "nhanh");
        ok &= expectSelection("a key before the text arrived", selection.take(), std::nullopt);

        ticket = selection.announce();
        selection.arrive(selection.announce(), "abc nhanh xyz");
        selection.arrive(ticket, "nhanh");
        ok &= expectSelection("an older selection's text after a newer one", selection.take(), "abc nhanh xyz");
        return ok;
    }

    struct Cookie {
        unsigned int sequence;
    };
    struct ScreenIterator {
        const uint32_t* data;
        int             rem;
        int             index;
    };

    uint32_t word32(const uint8_t* data, size_t offset) {
        uint32_t value = 0;
        std::memcpy(&value, data + offset, sizeof(value));
        return value;
    }

    // An X11 app with two windows on the test display, through xcb loaded at run time like the watcher.
    class FakeApp {
      public:
        bool open() {
            void* xcb = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
            if (xcb == nullptr)
                return false;
            load(xcb);
            conn_ = connect_(nullptr, nullptr);
            root_ = *roots_(getSetup_(conn_)).data;
            for (uint32_t* window : {&field_, &other_}) {
                *window = generateId_(conn_);
                create_(conn_, 0, *window, root_, 0, 0, 10, 10, 0, 1, 0, 0, nullptr);
                map_(conn_, *window);
            }
            uint8_t* answer = waitReply_(conn_, intern_(conn_, 0, 18, "_NET_ACTIVE_WINDOW").sequence, nullptr);
            active_         = answer ? word32(answer, 8) : 0;
            std::free(answer);
            focus_(conn_, 1, field_, 0);
            activate(field_);
            return true;
        }
        ~FakeApp() {
            if (conn_ != nullptr)
                disconnect_(conn_);
        }
        uint32_t field() const {
            return field_;
        }
        uint32_t other() const {
            return other_;
        }

        // Sets the active window on the root, as a window manager does.
        void activate(uint32_t window) {
            changeProp_(conn_, 0, root_, active_, 33, 32, 1, &window);
            sync();
        }

        // Selects text in the field and answers the first request for it.
        bool select(const std::string& text) {
            setOwner_(conn_, field_, 1, 0);
            sync();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (std::chrono::steady_clock::now() < deadline) {
                uint8_t* event = pollForEvent_(conn_);
                if (event == nullptr) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                const bool request = (event[0] & 0x7f) == 30;
                if (request)
                    answer(event, text);
                std::free(event);
                if (request)
                    return true;
            }
            return false;
        }

      private:
        template <typename T>
        static void get(void* lib, T& fn, const char* name) {
            fn = reinterpret_cast<T>(dlsym(lib, name));
        }
        void load(void* xcb) {
            get(xcb, connect_, "xcb_connect");
            get(xcb, getSetup_, "xcb_get_setup");
            get(xcb, roots_, "xcb_setup_roots_iterator");
            get(xcb, create_, "xcb_create_window");
            get(xcb, map_, "xcb_map_window");
            get(xcb, intern_, "xcb_intern_atom");
            get(xcb, focus_, "xcb_set_input_focus");
            get(xcb, generateId_, "xcb_generate_id");
            get(xcb, waitReply_, "xcb_wait_for_reply");
            get(xcb, getFocus_, "xcb_get_input_focus");
            get(xcb, setOwner_, "xcb_set_selection_owner");
            get(xcb, changeProp_, "xcb_change_property");
            get(xcb, sendEvent_, "xcb_send_event");
            get(xcb, pollForEvent_, "xcb_poll_for_event");
            get(xcb, disconnect_, "xcb_disconnect");
        }
        void sync() {
            std::free(waitReply_(conn_, getFocus_(conn_).sequence, nullptr));
        }

        // xcb_selection_request_event_t: requestor at 12, selection at 16, target at 20, property at 24.
        void answer(const uint8_t* request, const std::string& text) {
            const uint32_t requestor = word32(request, 12);
            const uint32_t target    = word32(request, 20);
            const uint32_t property  = word32(request, 24);
            changeProp_(conn_, 0, requestor, property, target, 8, static_cast<uint32_t>(text.size()), text.data());
            // xcb_selection_notify_event_t: requestor at 8, selection at 12, target at 16, property at 20.
            char notify[32] = {31};
            std::memcpy(notify + 8, &requestor, 4);
            std::memcpy(notify + 12, request + 16, 4);
            std::memcpy(notify + 16, &target, 4);
            std::memcpy(notify + 20, &property, 4);
            sendEvent_(conn_, 0, requestor, 0, notify);
            sync();
        }

        void*    conn_   = nullptr;
        uint32_t root_   = 0;
        uint32_t field_  = 0;
        uint32_t other_  = 0;
        uint32_t active_ = 0;
        void* (*connect_)(const char*, int*);
        const void* (*getSetup_)(void*);
        ScreenIterator (*roots_)(const void*);
        Cookie (*create_)(void*, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, const void*);
        Cookie (*map_)(void*, uint32_t);
        Cookie (*intern_)(void*, uint8_t, uint16_t, const char*);
        Cookie (*focus_)(void*, uint8_t, uint32_t, uint32_t);
        uint32_t (*generateId_)(void*);
        uint8_t* (*waitReply_)(void*, unsigned int, void**);
        Cookie (*getFocus_)(void*);
        Cookie (*setOwner_)(void*, uint32_t, uint32_t, uint32_t);
        Cookie (*changeProp_)(void*, uint8_t, uint32_t, uint32_t, uint32_t, uint8_t, uint32_t, const void*);
        Cookie (*sendEvent_)(void*, uint8_t, uint32_t, uint32_t, const char*);
        uint8_t* (*pollForEvent_)(void*);
        void (*disconnect_)(void*);
    };

    // A key after a selection gets its text, unless another window became active in between.
    bool keepsSelectionInActiveWindow() {
        std::string display;
        const pid_t xvfb = startXvfb(display);
        if (xvfb < 0) {
            std::cerr << "Xvfb not found: skipping the active window step\n";
            return true;
        }
        setenv("DISPLAY", display.c_str(), 1);
        bool ok = true;
        {
            FakeApp              app;
            ngosen::X11Selection selection;
            std::atomic<bool>    stop{false};
            ok = app.open();
            std::thread watcher([&] { ngosen::watchX11PointerClicks(stop, [] {}, &selection); });
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            const auto selectThenActivate = [&](uint32_t active) {
                ok &= app.select("nhanh");
                app.activate(active);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return selection.take();
            };
            ok &= expectSelection("the same window made active again", selectThenActivate(app.field()), "nhanh");
            ok &= expectSelection("another window made active", selectThenActivate(app.other()), std::nullopt);
            stop.store(true);
            watcher.join();
        }
        kill(xvfb, SIGTERM);
        waitpid(xvfb, nullptr, 0);
        setenv("DISPLAY", "", 1);
        return ok;
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
    if (ngosen::watchX11PointerClicks(stop, [&clicks] { ++clicks; })) {
        std::cerr << "Step: watch without an X11 session\nExpected: returns false\nActual: returned true\n";
        ok = false;
    }
    ok &= watchesXwaylandClicks();
    ok &= keepsSelection();
    ok &= keepsSelectionInActiveWindow();
    return ok ? 0 : 1;
}
