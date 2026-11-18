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
    std::function<void()> on_click;

    // Runs on the monitor thread, so on_click must hand its work to the event loop.
    void markMouseClick() {
        needEngineReset.store(true, std::memory_order_release);
        g_mouse_clicked.store(true, std::memory_order_release);
        if (on_click)
            on_click();
    }
} // namespace

void mousePressResetThread() {
    // Wayland reports clicks to no one but the app; checkCursorJump covers that case. Xwayland still
    // reports clicks on X11 windows.
    // Xwayland's primary selection also carries what Wayland apps select, which no X11 window owns.
    ngosen::watchX11PointerClicks(stop_flag_monitor, markMouseClick, getEnv("WAYLAND_DISPLAY").empty() ? &x11_selection : nullptr);
}

void startMouseReset(std::function<void()> onClick) {
    on_click     = std::move(onClick);
    mouse_thread = std::thread(mousePressResetThread);
}
