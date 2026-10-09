/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

// The X server reports raw button presses to any client, so clicks need no /dev/input access.

// Calls onClick on each non-wheel button press until stop is set. Returns false at once without an
// X server or XInput2.
bool watchX11PointerClicks(const std::atomic<bool>& stop, const std::function<void()>& onClick);

// True for an XInput2 raw button press other than the wheel (buttons 4 to 7).
bool isRawClickEvent(const uint8_t* event, uint8_t xinputOpcode);
