/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-ibus-clicks.h"

#include "ngosen-globals.h"
#include "ngosen-log.h"
#include "ngosen-pointer.h"

namespace ngosen {

    ClickWatch::ClickWatch() :
        thread_([this] {
            const bool watched = watchX11PointerClicks(stop_, [] {
                needEngineReset.store(true, std::memory_order_release);
                g_mouse_clicked.store(true, std::memory_order_release);
            });
            if (!watched)
                NGOSEN_DEBUG("Clicks not watched: no X server with XInput2");
        }) {}

    ClickWatch::~ClickWatch() {
        stop_.store(true, std::memory_order_release);
        if (thread_.joinable())
            thread_.join();
    }

} // namespace ngosen
