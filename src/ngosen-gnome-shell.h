/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

#ifdef NGOSEN_HAS_DBUS
#include <fcitx-utils/dbus/bus.h>
#include <fcitx-utils/dbus/servicewatcher.h>

#include <functional>
#include <memory>
#endif

namespace ngosen {

    // GNOME Shell 50.0 to 50.3 lose the keys an input method forwards (mutter#4853).
    bool gnomeShellDropsForwardedKeys(const std::string& version);

    // The name of the app a client stands for: fcitx5 before 5.1.22 names every app GNOME Shell relays after the shell.
    const std::string& relayedAppName(const std::string& program, const std::string& frontend, const std::string* shellFocusedApp);

#ifdef NGOSEN_HAS_DBUS
    // Whether GNOME Shell loses forwarded keys right now: a dropping version without forward-keys@ngosen.github.io running.
    // Also passes on the focused app that extension reports, empty when nothing does.
    class GnomeShellWatcher {
      public:
        GnomeShellWatcher(fcitx::dbus::Bus* bus, std::function<void(const std::string&)> onFocusedApp);

        const bool* dropsKeys() const {
            return &drops_keys_;
        }

      private:
        void                                              refresh();
        void                                              update();
        void                                              askFocusedApp();
        void                                              setFocusedApp(const std::string& app);

        fcitx::dbus::Bus*                                 bus_;
        fcitx::dbus::ServiceWatcher                       watcher_;
        std::unique_ptr<fcitx::dbus::ServiceWatcherEntry> owner_entry_;
        std::unique_ptr<fcitx::dbus::Slot>                state_signal_;
        std::unique_ptr<fcitx::dbus::Slot>                version_call_;
        std::unique_ptr<fcitx::dbus::Slot>                extension_call_;
        std::unique_ptr<fcitx::dbus::Slot>                app_signal_;
        std::unique_ptr<fcitx::dbus::Slot>                app_call_;
        std::function<void(const std::string&)>           on_focused_app_;
        std::string                                       focused_app_;
        std::string                                       version_;
        bool                                              extension_active_ = false;
        bool                                              drops_keys_       = false;
    };
#endif

} // namespace ngosen
