// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-pointer.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

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
    return ok ? 0 : 1;
}
