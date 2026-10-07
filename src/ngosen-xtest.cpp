/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-xtest.h"
#include "lotus-utils.h"

#include <cstdint>
#include <dlfcn.h>
#include <utility>

namespace {
    // Loaded at run time so a system without the library still builds and runs.
    struct XcbConnection;
    struct XcbVoidCookie {
        unsigned int sequence;
    };
    using ConnectFn   = XcbConnection* (*)(const char*, int*);
    using HasErrorFn  = int (*)(XcbConnection*);
    using FlushFn     = int (*)(XcbConnection*);
    using FakeInputFn = XcbVoidCookie (*)(XcbConnection*, uint8_t, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint8_t);

    constexpr uint8_t KeyPress   = 2;
    constexpr uint8_t KeyRelease = 3;
    // XKB keycodes: evdev code + 8.
    constexpr uint8_t BackSpaceCode = 22;
    constexpr uint8_t LeftCode      = 113;
    // Right Shift: fcitx5 treats a lone left Shift tap as the switch to English.
    constexpr uint8_t RightShiftCode = 62;

    class XTest {
      public:
        XTest() {
            if (getEnv("DISPLAY").empty() || !getEnv("WAYLAND_DISPLAY").empty()) {
                return;
            }
            void* xcb   = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
            void* xtest = dlopen("libxcb-xtest.so.0", RTLD_NOW | RTLD_LOCAL);
            if (xcb == nullptr || xtest == nullptr) {
                LOTUS_WARN("XTEST unavailable: cannot load libxcb-xtest");
                return;
            }
            auto connect  = reinterpret_cast<ConnectFn>(dlsym(xcb, "xcb_connect"));
            auto hasError = reinterpret_cast<HasErrorFn>(dlsym(xcb, "xcb_connection_has_error"));
            auto flush    = reinterpret_cast<FlushFn>(dlsym(xcb, "xcb_flush"));
            auto fake     = reinterpret_cast<FakeInputFn>(dlsym(xtest, "xcb_test_fake_input"));
            if (connect == nullptr || hasError == nullptr || flush == nullptr || fake == nullptr) {
                LOTUS_WARN("XTEST unavailable: missing xcb symbols");
                return;
            }
            XcbConnection* conn = connect(nullptr, nullptr);
            if (conn == nullptr || hasError(conn) != 0) {
                LOTUS_WARN("XTEST unavailable: cannot open the display");
                return;
            }
            conn_      = conn;
            flush_     = flush;
            fakeInput_ = fake;
        }

        bool available() const {
            return fakeInput_ != nullptr;
        }

        void send(int count) const {
            if (count > 0) {
                for (int i = 0; i < count; ++i) {
                    tap(BackSpaceCode);
                }
            } else {
                fakeInput_(conn_, KeyPress, RightShiftCode, 0, 0, 0, 0, 0);
                for (int i = 0; i < -count; ++i) {
                    tap(LeftCode);
                }
                fakeInput_(conn_, KeyRelease, RightShiftCode, 0, 0, 0, 0, 0);
            }
            flush_(conn_);
        }

      private:
        void tap(uint8_t code) const {
            fakeInput_(conn_, KeyPress, code, 0, 0, 0, 0, 0);
            fakeInput_(conn_, KeyRelease, code, 0, 0, 0, 0, 0);
        }

        XcbConnection* conn_      = nullptr;
        FlushFn        flush_     = nullptr;
        FakeInputFn    fakeInput_ = nullptr;
    };

    const XTest& display() {
        static const XTest instance;
        return instance;
    }

    std::function<bool(int)>& testSender() {
        static std::function<bool(int)> sender;
        return sender;
    }
} // namespace

bool xtestAvailable() {
    return testSender() ? true : display().available();
}

bool xtestSendKeys(int count) {
    if (testSender()) {
        return testSender()(count);
    }
    if (!display().available() || count == 0) {
        return false;
    }
    display().send(count);
    return true;
}

void setXTestSenderForTest(std::function<bool(int)> sender) {
    testSender() = std::move(sender);
}
