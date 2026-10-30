/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

// The X server reports raw button presses to any client, so clicks need no /dev/input access.

// The text last selected in the focused X11 window. A click forgets it and a key takes it, so only
// the key right after a selection gets it.
class X11Selection {
  public:
    // A new selection, whose text arrives later under the returned ticket.
    uint64_t                   announce();
    void                       arrive(uint64_t ticket, std::string text);
    void                       forget();
    std::optional<std::string> take();

  private:
    std::mutex                 mutex_;
    uint64_t                   ticket_ = 0;
    std::optional<std::string> text_;
};

// Calls onClick on each non-wheel button press until stop is set, and keeps selection up to date
// when given. Returns false at once without an X server or XInput2.
bool watchX11PointerClicks(const std::atomic<bool>& stop, const std::function<void()>& onClick, X11Selection* selection = nullptr);

// True for an XInput2 raw button press other than the wheel (buttons 4 to 7).
bool isRawClickEvent(const uint8_t* event, uint8_t xinputOpcode);
