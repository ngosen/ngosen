/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "ngosen-monitor.h"
#include "ngosen-utils.h"
#include "ngosen-pointer.h"

std::thread          mouse_thread = std::thread();
ngosen::X11Selection x11_selection;

namespace {
    void markMouseClick() {
        needEngineReset.store(true, std::memory_order_release);
        g_mouse_clicked.store(true, std::memory_order_release);
    }
} // namespace

void mousePressResetThread() {
    // Wayland reports clicks to no one but the app; checkCursorJump covers that case. Xwayland still
    // reports clicks on X11 windows.
    // Xwayland's primary selection also carries what Wayland apps select, which no X11 window owns.
    ngosen::watchX11PointerClicks(stop_flag_monitor, markMouseClick, getEnv("WAYLAND_DISPLAY").empty() ? &x11_selection : nullptr);
}

void startMouseReset() {
    mouse_thread = std::thread(mousePressResetThread);
}
