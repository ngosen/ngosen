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

    uint32_t word32(const uint8_t* data, size_t offset) {
        uint32_t value = 0;
        std::memcpy(&value, data + offset, sizeof(value));
        return value;
    }

    using GenerateIdFn   = uint32_t (*)(XcbConnection*);
    using CreateWindowFn = XcbCookie (*)(XcbConnection*, uint8_t, uint32_t, uint32_t, int16_t, int16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, const void*);
    using ChangeAttributesFn = XcbCookie (*)(XcbConnection*, uint32_t, uint32_t, const void*);
    using InternAtomFn       = XcbCookie (*)(XcbConnection*, uint8_t, uint16_t, const char*);
    using ConnectionCallFn   = XcbCookie (*)(XcbConnection*);
    using WaitForReplyFn     = uint8_t* (*)(XcbConnection*, unsigned int, void**);
    using ConvertSelectionFn = XcbCookie (*)(XcbConnection*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    using GetPropertyFn      = XcbCookie (*)(XcbConnection*, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    using FixesVersionFn     = XcbCookie (*)(XcbConnection*, uint32_t, uint32_t);
    using FixesSelectFn      = XcbCookie (*)(XcbConnection*, uint32_t, uint32_t, uint32_t);

    constexpr uint8_t  PropertyNotify   = 28;
    constexpr uint8_t  SelectionNotify  = 31;
    constexpr uint32_t PrimaryAtom      = 1;
    constexpr uint32_t PropertyChange   = 1U << 22;
    constexpr uint32_t EventMaskBit     = 1U << 11;
    constexpr uint16_t InputOnly        = 2;
    constexpr uint32_t SetOwnerMask     = 1;
    constexpr uint32_t MaxSelectionLong = 64; // a word, not a page

    // Keeps an X11Selection up to date: XFixes reports each new owner of the primary selection, and
    // its text is then asked of that owner.
    class SelectionFollower {
      public:
        bool open(void* xcb, XcbConnection* conn, uint32_t root, X11Selection* selection);
        void handle(const uint8_t* event);

      private:
        uint8_t*           reply(XcbCookie cookie);
        uint32_t           atom(const char* name);
        bool               ownedByFocusedClient(uint32_t owner);
        void               ownerChanged(uint32_t owner);
        void               converted(uint32_t property);

        XcbConnection*     conn_       = nullptr;
        X11Selection*      selection_  = nullptr;
        FlushFn            flush_      = nullptr;
        WaitForReplyFn     wait_       = nullptr;
        InternAtomFn       intern_     = nullptr;
        ConnectionCallFn   getFocus_   = nullptr;
        ConvertSelectionFn convert_    = nullptr;
        GetPropertyFn      getProp_    = nullptr;
        uint8_t            firstEvent_ = 0;
        uint32_t           clientMask_ = 0;
        uint32_t           window_     = 0;
        uint32_t           utf8_       = 0;
        uint32_t           property_   = 0;
        uint32_t           active_     = 0;
        uint64_t           ticket_     = 0;
    };

    bool SelectionFollower::open(void* xcb, XcbConnection* conn, uint32_t root, X11Selection* selection) {
        void* xfixes        = dlopen("libxcb-xfixes.so.0", RTLD_NOW | RTLD_LOCAL);
        auto  extData       = symbol<ExtensionDataFn>(xcb, "xcb_get_extension_data");
        auto  getSetup      = symbol<GetSetupFn>(xcb, "xcb_get_setup");
        auto  generateId    = symbol<GenerateIdFn>(xcb, "xcb_generate_id");
        auto  createWindow  = symbol<CreateWindowFn>(xcb, "xcb_create_window");
        auto  changeAttribs = symbol<ChangeAttributesFn>(xcb, "xcb_change_window_attributes");
        auto  fixesVersion  = xfixes ? symbol<FixesVersionFn>(xfixes, "xcb_xfixes_query_version") : nullptr;
        auto  fixesSelect   = xfixes ? symbol<FixesSelectFn>(xfixes, "xcb_xfixes_select_selection_input") : nullptr;
        auto* fixesId       = xfixes ? symbol<XcbExtension*>(xfixes, "xcb_xfixes_id") : nullptr;
        conn_               = conn;
        selection_          = selection;
        flush_              = symbol<FlushFn>(xcb, "xcb_flush");
        wait_               = symbol<WaitForReplyFn>(xcb, "xcb_wait_for_reply");
        intern_             = symbol<InternAtomFn>(xcb, "xcb_intern_atom");
        getFocus_           = symbol<ConnectionCallFn>(xcb, "xcb_get_input_focus");
        convert_            = symbol<ConvertSelectionFn>(xcb, "xcb_convert_selection");
        getProp_            = symbol<GetPropertyFn>(xcb, "xcb_get_property");
        const uint8_t* ext  = fixesId && extData ? extData(conn, fixesId) : nullptr;
        if (!ext || ext[8] == 0 || !getSetup || !generateId || !createWindow || !changeAttribs || !fixesVersion || !fixesSelect || !flush_ || !wait_ || !intern_ || !getFocus_ ||
            !convert_ || !getProp_) {
            NGOSEN_WARN("Primary selection unavailable: missing XFixes");
            return false;
        }
        std::free(reply(fixesVersion(conn, 5, 0)));
        firstEvent_ = ext[10];
        // xcb_setup_t: resource_id_mask at byte 16; ids of one client differ only in these bits.
        clientMask_ = word32(static_cast<const uint8_t*>(getSetup(conn)), 16);
        utf8_       = atom("UTF8_STRING");
        property_   = atom("NGOSEN_SELECTION");
        active_     = atom("_NET_ACTIVE_WINDOW");
        window_     = generateId(conn);
        createWindow(conn, 0, window_, root, 0, 0, 1, 1, 0, InputOnly, 0, 0, nullptr);
        changeAttribs(conn, root, EventMaskBit, &PropertyChange);
        fixesSelect(conn, root, PrimaryAtom, SetOwnerMask);
        flush_(conn);
        return true;
    }

    // An error comes back as an event, which the follower ignores.
    uint8_t* SelectionFollower::reply(XcbCookie cookie) {
        return wait_(conn_, cookie.sequence, nullptr);
    }

    uint32_t SelectionFollower::atom(const char* name) {
        uint8_t* answer = reply(intern_(conn_, 0, static_cast<uint16_t>(std::strlen(name)), name));
        // xcb_intern_atom_reply_t: atom at byte 8.
        const uint32_t value = answer ? word32(answer, 8) : 0;
        std::free(answer);
        return value;
    }

    void SelectionFollower::handle(const uint8_t* event) {
        const uint8_t type = event[0] & 0x7f;
        if (type == firstEvent_) // XFixesSelectionNotify: owner at byte 8
            ownerChanged(word32(event, 8));
        else if (type == PropertyNotify && word32(event, 8) == active_) // atom at byte 8
            selection_->forget();
        else if (type == SelectionNotify && word32(event, 8) == window_) // requestor at 8, property at 20
            converted(word32(event, 20));
    }

    bool SelectionFollower::ownedByFocusedClient(uint32_t owner) {
        uint8_t* answer = reply(getFocus_(conn_));
        // xcb_get_input_focus_reply_t: focus at byte 8; 0 is None and 1 is PointerRoot.
        const uint32_t focus = answer ? word32(answer, 8) : 0;
        std::free(answer);
        return focus > 1 && (focus & ~clientMask_) == (owner & ~clientMask_);
    }

    void SelectionFollower::ownerChanged(uint32_t owner) {
        // Another window's selection says nothing about the field being typed in.
        if (owner == 0 || !ownedByFocusedClient(owner)) {
            selection_->forget();
            return;
        }
        ticket_ = selection_->announce();
        convert_(conn_, window_, PrimaryAtom, utf8_, property_, 0);
        flush_(conn_);
    }

    void SelectionFollower::converted(uint32_t property) {
        if (property == 0)
            return;
        uint8_t* answer = reply(getProp_(conn_, 1, window_, property_, 0, 0, MaxSelectionLong));
        // xcb_get_property_reply_t: format at 1, type at 8, bytes_after at 12, value_len at 16, value at 32.
        if (answer && answer[1] == 8 && word32(answer, 8) == utf8_ && word32(answer, 12) == 0)
            selection_->arrive(ticket_, std::string(reinterpret_cast<const char*>(answer + 32), word32(answer, 16)));
        std::free(answer);
    }
} // namespace

uint64_t X11Selection::announce() {
    std::lock_guard lock(mutex_);
    text_.reset();
    return ++ticket_;
}

void X11Selection::arrive(uint64_t ticket, std::string text) {
    std::lock_guard lock(mutex_);
    if (ticket == ticket_)
        text_ = std::move(text);
}

void X11Selection::forget() {
    std::lock_guard lock(mutex_);
    text_.reset();
    ++ticket_;
}

std::optional<std::string> X11Selection::take() {
    std::lock_guard lock(mutex_);
    ++ticket_;
    return std::exchange(text_, std::nullopt);
}

bool isRawClickEvent(const uint8_t* event, uint8_t xinputOpcode) {
    if ((event[0] & 0x7f) != GeGeneric || event[1] != xinputOpcode)
        return false;
    uint16_t type   = 0;
    uint32_t button = 0;
    std::memcpy(&type, event + 8, sizeof(type));
    std::memcpy(&button, event + 16, sizeof(button));
    return type == RawButtonPress && (button < 4 || button > 7);
}

bool watchX11PointerClicks(const std::atomic<bool>& stop, const std::function<void()>& onClick, X11Selection* selection) {
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
    // On one connection, so a click and a selection arrive in the order they happened.
    SelectionFollower follower;
    auto              getSetup = symbol<GetSetupFn>(xcb, "xcb_get_setup");
    auto              roots    = symbol<RootsIteratorFn>(xcb, "xcb_setup_roots_iterator");
    if (selection != nullptr && !follower.open(xcb, conn, *roots(getSetup(conn)).data, selection))
        selection = nullptr;

    pollfd pfd{getFd(conn), POLLIN, 0};
    // The timeout bounds how long shutdown waits for this thread.
    while (!stop.load(std::memory_order_acquire) && hasError(conn) == 0) {
        if (poll(&pfd, 1, 250) < 0 && errno != EINTR)
            break;
        while (uint8_t* event = pollForEvent(conn)) {
            if (isRawClickEvent(event, opcode)) {
                onClick();
                if (selection != nullptr)
                    selection->forget();
            } else if (selection != nullptr)
                follower.handle(event);
            std::free(event);
        }
    }
    disconnect(conn);
    return true;
}
