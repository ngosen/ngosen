/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-pointer.h"
#include "ngosen-utils.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <poll.h>

namespace {
    // Loaded at run time, like XTEST, so a system without libxcb-xinput still builds and runs.
    struct XcbConnection;
    struct XcbExtension;
    struct XcbCookie {
        unsigned int sequence;
    };
    struct XcbScreenIterator {
        const uint32_t* data; // xcb_screen_t starts with the root window id
        int             rem;
        int             index;
    };
    struct XcbEventMask {
        uint16_t deviceid;
        uint16_t maskLen;
        uint32_t mask;
    };

    using ConnectFn       = XcbConnection* (*)(const char*, int*);
    using DisconnectFn    = void (*)(XcbConnection*);
    using HasErrorFn      = int (*)(XcbConnection*);
    using FlushFn         = int (*)(XcbConnection*);
    using GetFdFn         = int (*)(XcbConnection*);
    using PollForEventFn  = uint8_t* (*)(XcbConnection*);
    using GetSetupFn      = const void* (*)(XcbConnection*);
    using RootsIteratorFn = XcbScreenIterator (*)(const void*);
    using ExtensionDataFn = const uint8_t* (*)(XcbConnection*, XcbExtension*);
    using RequestCheckFn  = void* (*)(XcbConnection*, XcbCookie);
    using QueryVersionFn  = XcbCookie (*)(XcbConnection*, uint16_t, uint16_t);
    using VersionReplyFn  = uint8_t* (*)(XcbConnection*, XcbCookie, void**);
    using SelectEventsFn  = XcbCookie (*)(XcbConnection*, uint32_t, uint16_t, const XcbEventMask*);

    constexpr uint8_t  GeGeneric         = 35;
    constexpr uint16_t RawButtonPress    = 15;
    constexpr uint16_t AllMasterDevices  = 1;
    constexpr uint32_t RawButtonPressBit = 1U << RawButtonPress;

    template <typename T>
    T symbol(void* lib, const char* name) {
        return reinterpret_cast<T>(dlsym(lib, name));
    }

    // Opens a connection that receives raw button presses; nullptr when anything is missing.
    XcbConnection* openRawButtonConnection(void* xcb, void* xinput, uint8_t& opcode) {
        auto  connect      = symbol<ConnectFn>(xcb, "xcb_connect");
        auto  disconnect   = symbol<DisconnectFn>(xcb, "xcb_disconnect");
        auto  hasError     = symbol<HasErrorFn>(xcb, "xcb_connection_has_error");
        auto  getSetup     = symbol<GetSetupFn>(xcb, "xcb_get_setup");
        auto  roots        = symbol<RootsIteratorFn>(xcb, "xcb_setup_roots_iterator");
        auto  extData      = symbol<ExtensionDataFn>(xcb, "xcb_get_extension_data");
        auto  requestCheck = symbol<RequestCheckFn>(xcb, "xcb_request_check");
        auto  queryVersion = symbol<QueryVersionFn>(xinput, "xcb_input_xi_query_version");
        auto  versionReply = symbol<VersionReplyFn>(xinput, "xcb_input_xi_query_version_reply");
        auto  select       = symbol<SelectEventsFn>(xinput, "xcb_input_xi_select_events_checked");
        auto* inputId      = symbol<XcbExtension*>(xinput, "xcb_input_id");
        if (!connect || !disconnect || !hasError || !getSetup || !roots || !extData || !requestCheck || !queryVersion || !versionReply || !select || !inputId) {
            NGOSEN_WARN("XInput2 unavailable: missing xcb symbols");
            return nullptr;
        }

        XcbConnection* conn = connect(nullptr, nullptr);
        if (conn == nullptr || hasError(conn) != 0) {
            NGOSEN_WARN("XInput2 unavailable: cannot open the display");
            if (conn != nullptr)
                disconnect(conn);
            return nullptr;
        }
        const uint8_t* ext = extData(conn, inputId);
        // xcb_query_extension_reply_t: present at byte 8, major_opcode at byte 9.
        // From 2.1 on, raw events arrive even while another client grabs the pointer, as Chromium does on press.
        uint8_t* version = (ext != nullptr && ext[8] != 0) ? versionReply(conn, queryVersion(conn, 2, 2), nullptr) : nullptr;
        // xcb_input_xi_query_version_reply_t: major_version at byte 8.
        uint16_t major = 0;
        if (version != nullptr)
            std::memcpy(&major, version + 8, sizeof(major));
        std::free(version);
        if (major < 2) {
            NGOSEN_WARN("XInput2 unavailable: the X server lacks XInput 2");
            disconnect(conn);
            return nullptr;
        }

        const XcbEventMask mask{AllMasterDevices, 1, RawButtonPressBit};
        void*              error = requestCheck(conn, select(conn, *roots(getSetup(conn)).data, 1, &mask));
        if (error != nullptr) {
            std::free(error);
            NGOSEN_WARN("XInput2 unavailable: cannot select raw button events");
            disconnect(conn);
            return nullptr;
        }
        opcode = ext[9];
        return conn;
    }
} // namespace

bool isRawClickEvent(const uint8_t* event, uint8_t xinputOpcode) {
    if ((event[0] & 0x7f) != GeGeneric || event[1] != xinputOpcode)
        return false;
    uint16_t type   = 0;
    uint32_t button = 0;
    std::memcpy(&type, event + 8, sizeof(type));
    std::memcpy(&button, event + 16, sizeof(button));
    return type == RawButtonPress && (button < 4 || button > 7);
}

bool watchX11PointerClicks(const std::atomic<bool>& stop, const std::function<void()>& onClick) {
    if (getEnv("DISPLAY").empty())
        return false;
    void* xcb    = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
    void* xinput = dlopen("libxcb-xinput.so.0", RTLD_NOW | RTLD_LOCAL);
    if (xcb == nullptr || xinput == nullptr) {
        NGOSEN_WARN("XInput2 unavailable: cannot load libxcb-xinput");
        return false;
    }
    uint8_t        opcode = 0;
    XcbConnection* conn   = openRawButtonConnection(xcb, xinput, opcode);
    if (conn == nullptr)
        return false;
    auto getFd        = symbol<GetFdFn>(xcb, "xcb_get_file_descriptor");
    auto pollForEvent = symbol<PollForEventFn>(xcb, "xcb_poll_for_event");
    auto hasError     = symbol<HasErrorFn>(xcb, "xcb_connection_has_error");
    auto disconnect   = symbol<DisconnectFn>(xcb, "xcb_disconnect");
    NGOSEN_INFO("Watching mouse clicks through XInput2.");

    pollfd pfd{getFd(conn), POLLIN, 0};
    // The timeout bounds how long shutdown waits for this thread.
    while (!stop.load(std::memory_order_acquire) && hasError(conn) == 0) {
        if (poll(&pfd, 1, 250) < 0 && errno != EINTR)
            break;
        while (uint8_t* event = pollForEvent(conn)) {
            if (isRawClickEvent(event, opcode))
                onClick();
            std::free(event);
        }
    }
    disconnect(conn);
    return true;
}
