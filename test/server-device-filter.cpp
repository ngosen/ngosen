// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit test for shouldOpenInputDevice(): the uinput server opens pointers only, never a node that
// reports typed keys.

#include "lotus-device-filter.h"

#include <iostream>
#include <map>
#include <string>

namespace {

    int  failures = 0;

    void expect(const std::string& name, const std::map<std::string, std::string>& properties, bool open) {
        const bool got = shouldOpenInputDevice([&properties](const char* key) -> const char* {
            auto it = properties.find(key);
            return it == properties.end() ? nullptr : it->second.c_str();
        });
        if (got != open) {
            std::cerr << "FAIL: " << name << '\n';
            ++failures;
        }
    }

} // namespace

int main() {
    expect("mouse", {{"ID_INPUT", "1"}, {"ID_INPUT_MOUSE", "1"}}, true);
    expect("touchpad", {{"ID_INPUT", "1"}, {"ID_INPUT_TOUCHPAD", "1"}}, true);
    expect("pointing stick", {{"ID_INPUT", "1"}, {"ID_INPUT_POINTINGSTICK", "1"}}, true);

    expect("keyboard", {{"ID_INPUT", "1"}, {"ID_INPUT_KEY", "1"}, {"ID_INPUT_KEYBOARD", "1"}}, false);
    expect("keyboard with touchpad on one node", {{"ID_INPUT_KEYBOARD", "1"}, {"ID_INPUT_TOUCHPAD", "1"}}, false);
    expect("mouse that also types", {{"ID_INPUT_KEYBOARD", "1"}, {"ID_INPUT_MOUSE", "1"}}, false);
    expect("power button", {{"ID_INPUT", "1"}, {"ID_INPUT_KEY", "1"}}, false);
    expect("lid switch", {{"ID_INPUT", "1"}, {"ID_INPUT_SWITCH", "1"}}, false);
    expect("touchscreen", {{"ID_INPUT", "1"}, {"ID_INPUT_TOUCHSCREEN", "1"}}, false);
    expect("mouse flag set to 0", {{"ID_INPUT_MOUSE", "0"}}, false);
    expect("no properties", {}, false);

    return failures == 0 ? 0 : 1;
}
