/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <atomic>
#include <thread>

namespace ngosen {

    // A click on an X11 window can move the cursor to another cell or field without a focus change, so
    // the word being typed ends there. Xwayland reports such clicks in a Wayland session too.
    class ClickWatch {
      public:
        ClickWatch();
        ~ClickWatch();
        ClickWatch(const ClickWatch&)            = delete;
        ClickWatch& operator=(const ClickWatch&) = delete;

      private:
        std::atomic<bool> stop_{false};
        std::thread       thread_;
    };

} // namespace ngosen
