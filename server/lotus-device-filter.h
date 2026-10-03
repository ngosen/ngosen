/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _LOTUS_DEVICE_FILTER_H_
#define _LOTUS_DEVICE_FILTER_H_

#include <cstring>

/**
 * @brief Whether the server may open an input device, given its udev properties.
 *
 * The server only needs pointer button presses, so it opens mice, touchpads and pointing sticks.
 * A node that also reports typed keys stays closed even if it is a pointer too: reading it would
 * let the server see what the user types.
 *
 * @param property Returns the udev property value for a key, or nullptr if unset.
 */
template <typename GetProperty>
bool shouldOpenInputDevice(GetProperty property) {
    const auto isSet = [&property](const char* key) {
        const char* value = property(key);
        return value != nullptr && std::strcmp(value, "1") == 0;
    };
    if (isSet("ID_INPUT_KEYBOARD")) {
        return false;
    }
    return isSet("ID_INPUT_MOUSE") || isSet("ID_INPUT_TOUCHPAD") || isSet("ID_INPUT_POINTINGSTICK");
}

#endif // _LOTUS_DEVICE_FILTER_H_
