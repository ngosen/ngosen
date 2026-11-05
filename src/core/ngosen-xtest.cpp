/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-xtest.h"
#include "ngosen-log.h"

#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <utility>

namespace {
    bool envSet(const char* name) {
        const char* value = std::getenv(name);
        return value != nullptr && *value != 0;
    }

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
            if (!envSet("DISPLAY") || envSet("WAYLAND_DISPLAY")) {
                return;
            }
            void* xcb   = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
            void* xtest = dlopen("libxcb-xtest.so.0", RTLD_NOW | RTLD_LOCAL);
            if (xcb == nullptr || xtest == nullptr) {
                NGOSEN_WARN("XTEST unavailable: cannot load libxcb-xtest");
                return;
            }
            auto connect  = reinterpret_cast<ConnectFn>(dlsym(xcb, "xcb_connect"));
            auto hasError = reinterpret_cast<HasErrorFn>(dlsym(xcb, "xcb_connection_has_error"));
            auto flush    = reinterpret_cast<FlushFn>(dlsym(xcb, "xcb_flush"));
            auto fake     = reinterpret_cast<FakeInputFn>(dlsym(xtest, "xcb_test_fake_input"));
            if (connect == nullptr || hasError == nullptr || flush == nullptr || fake == nullptr) {
                NGOSEN_WARN("XTEST unavailable: missing xcb symbols");
                return;
            }
            XcbConnection* conn = connect(nullptr, nullptr);
            if (conn == nullptr || hasError(conn) != 0) {
                NGOSEN_WARN("XTEST unavailable: cannot open the display");
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

    struct XcbGenericError;
    struct XcbFocusReply {
        uint8_t  responseType;
        uint8_t  revertTo;
        uint16_t sequence;
        uint32_t length;
        uint32_t focus;
    };
    struct XcbKeyEvent {
        uint8_t  responseType;
        uint8_t  detail;
        uint16_t sequence;
        uint32_t time;
        uint32_t root;
        uint32_t event;
        uint32_t child;
        int16_t  rootX;
        int16_t  rootY;
        int16_t  eventX;
        int16_t  eventY;
        uint16_t state;
        uint8_t  sameScreen;
        uint8_t  pad;
    };
    static_assert(sizeof(XcbKeyEvent) == 32, "X events are 32 bytes");
    struct XcbScreenIterator {
        const uint32_t* screen;
        int             rem;
        int             index;
    };
    using DisconnectFn    = void (*)(XcbConnection*);
    using GetSetupFn      = const void* (*)(XcbConnection*);
    using RootsIteratorFn = XcbScreenIterator (*)(const void*);
    using GetFocusFn      = XcbVoidCookie (*)(XcbConnection*);
    using FocusReplyFn    = XcbFocusReply* (*)(XcbConnection*, XcbVoidCookie, XcbGenericError**);
    using SendEventFn     = XcbVoidCookie (*)(XcbConnection*, uint8_t, uint32_t, uint32_t, const char*);

    constexpr uint32_t KeyPressMask   = 1;
    constexpr uint32_t KeyReleaseMask = 2;

    // Under Xwayland, XTEST input goes through the compositor, which asks the user first. Events sent to
    // the focused X window stay inside Xwayland and reach the client through its input method module like typed keys.
    class XSend {
      public:
        XSend() {
            if (!envSet("DISPLAY") || !envSet("WAYLAND_DISPLAY")) {
                return;
            }
            void* xcb = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
            if (xcb == nullptr) {
                NGOSEN_WARN("Cannot send X key events: cannot load libxcb");
                return;
            }
            connect_       = reinterpret_cast<ConnectFn>(dlsym(xcb, "xcb_connect"));
            hasError_      = reinterpret_cast<HasErrorFn>(dlsym(xcb, "xcb_connection_has_error"));
            flush_         = reinterpret_cast<FlushFn>(dlsym(xcb, "xcb_flush"));
            disconnect_    = reinterpret_cast<DisconnectFn>(dlsym(xcb, "xcb_disconnect"));
            getSetup_      = reinterpret_cast<GetSetupFn>(dlsym(xcb, "xcb_get_setup"));
            rootsIterator_ = reinterpret_cast<RootsIteratorFn>(dlsym(xcb, "xcb_setup_roots_iterator"));
            getFocus_      = reinterpret_cast<GetFocusFn>(dlsym(xcb, "xcb_get_input_focus"));
            focusReply_    = reinterpret_cast<FocusReplyFn>(dlsym(xcb, "xcb_get_input_focus_reply"));
            sendEvent_     = reinterpret_cast<SendEventFn>(dlsym(xcb, "xcb_send_event"));
            if (connect_ && hasError_ && flush_ && disconnect_ && getSetup_ && rootsIterator_ && getFocus_ && focusReply_ && sendEvent_) {
                open();
            } else {
                NGOSEN_WARN("Cannot send X key events: missing xcb symbols");
            }
        }

        bool available() const {
            return conn_ != nullptr;
        }

        bool send(int count) {
            // Xwayland restarts when the compositor stops it while no X client is left.
            if (hasError_(conn_) != 0) {
                disconnect_(conn_);
                conn_ = nullptr;
                open();
                if (conn_ == nullptr) {
                    return false;
                }
            }
            XcbFocusReply* reply = focusReply_(conn_, getFocus_(conn_), nullptr);
            if (reply == nullptr) {
                return false;
            }
            const uint32_t window = reply->focus;
            std::free(reply);
            // None (0) and PointerRoot (1) are not client windows.
            if (window <= 1) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                sendKey(window, KeyPress, KeyPressMask);
                sendKey(window, KeyRelease, KeyReleaseMask);
            }
            flush_(conn_);
            return true;
        }

      private:
        void open() {
            XcbConnection* conn = connect_(nullptr, nullptr);
            if (conn == nullptr || hasError_(conn) != 0) {
                NGOSEN_WARN("Cannot send X key events: cannot open the display");
                if (conn != nullptr) {
                    disconnect_(conn);
                }
                return;
            }
            // xcb_screen_t starts with the root window.
            root_ = *rootsIterator_(getSetup_(conn)).screen;
            conn_ = conn;
        }

        void sendKey(uint32_t window, uint8_t type, uint32_t mask) const {
            XcbKeyEvent event{};
            event.responseType = type;
            event.detail       = BackSpaceCode;
            event.root         = root_;
            event.event        = window;
            event.sameScreen   = 1;
            sendEvent_(conn_, 1, window, mask, reinterpret_cast<const char*>(&event));
        }

        XcbConnection*  conn_          = nullptr;
        uint32_t        root_          = 0;
        ConnectFn       connect_       = nullptr;
        HasErrorFn      hasError_      = nullptr;
        FlushFn         flush_         = nullptr;
        DisconnectFn    disconnect_    = nullptr;
        GetSetupFn      getSetup_      = nullptr;
        RootsIteratorFn rootsIterator_ = nullptr;
        GetFocusFn      getFocus_      = nullptr;
        FocusReplyFn    focusReply_    = nullptr;
        SendEventFn     sendEvent_     = nullptr;
    };

    const XTest& display() {
        static const XTest instance;
        return instance;
    }

    XSend& xwayland() {
        static XSend instance;
        return instance;
    }

    std::function<bool(int)>& testSender() {
        static std::function<bool(int)> sender;
        return sender;
    }

    std::function<bool(int)>& testXSender() {
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

bool xsendAvailable() {
    return testXSender() ? true : xwayland().available();
}

bool xsendKeys(int count) {
    if (testXSender()) {
        return testXSender()(count);
    }
    if (!xwayland().available() || count <= 0) {
        return false;
    }
    return xwayland().send(count);
}

void setXSendSenderForTest(std::function<bool(int)> sender) {
    testXSender() = std::move(sender);
}
