/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

#ifdef NGOSEN_HAS_DBUS
#include "ngosen-gnome-apps.h"

#include <fcitx-utils/dbus/bus.h>
#include <fcitx-utils/dbus/servicewatcher.h>

#include <functional>
#include <memory>
#endif

namespace ngosen {

    // GNOME Shell 50.0 to 50.3 lose the keys an input method forwards (mutter#4853).
    bool gnomeShellDropsForwardedKeys(const std::string& version);

    // Whether this fcitx5 version names the apps GNOME Shell relays by itself (5.1.22 and later).
    bool fcitxNamesGnomeApps(const std::string& version);

    // The name of the app a client stands for: fcitx5 before 5.1.22 names every app GNOME Shell relays after the shell, or not at all.
    const std::string& relayedAppName(const std::string& program, const std::string& frontend, const std::string* shellFocusedApp);

#ifdef NGOSEN_HAS_DBUS
    // Whether GNOME Shell loses forwarded keys right now: a dropping version without forward-keys@ngosen.github.io running.
    // Also passes on the focused app, empty when unknown: from that extension, else from GNOME's portal
    // when watchPortal is set, and the overview's search box while it is shown.
    class GnomeShellWatcher {
      public:
        GnomeShellWatcher(fcitx::dbus::Bus* bus, bool watchPortal, std::function<void(const std::string&)> onFocusedApp);

        const bool* dropsKeys() const {
            return &drops_keys_;
        }

      private:
        void                                              refresh();
        void                                              update();
        void                                              askFocusedApp();
        void                                              askOverview();
        void                                              publish();
        void                                              setOwners(const std::string& shell, const std::string& portal);
        void                                              startMonitor();
        bool                                              readMonitored(fcitx::dbus::Message& message);

        fcitx::dbus::Bus*                                 bus_;
        fcitx::dbus::ServiceWatcher                       watcher_;
        std::unique_ptr<fcitx::dbus::ServiceWatcherEntry> owner_entry_;
        std::unique_ptr<fcitx::dbus::ServiceWatcherEntry> portal_entry_;
        std::unique_ptr<fcitx::dbus::Slot>                state_signal_;
        std::unique_ptr<fcitx::dbus::Slot>                version_call_;
        std::unique_ptr<fcitx::dbus::Slot>                extension_call_;
        std::unique_ptr<fcitx::dbus::Slot>                app_signal_;
        std::unique_ptr<fcitx::dbus::Slot>                app_call_;
        std::unique_ptr<fcitx::dbus::Slot>                overview_signal_;
        std::unique_ptr<fcitx::dbus::Slot>                overview_call_;
        // Declared before its slots, which must go first.
        std::unique_ptr<fcitx::dbus::Bus>       monitor_bus_;
        std::unique_ptr<fcitx::dbus::Slot>      monitor_filter_;
        std::unique_ptr<fcitx::dbus::Slot>      monitor_call_;
        std::function<void(const std::string&)> on_focused_app_;
        GnomeAppNames                           names_;
        IntrospectReplyMatcher                  matcher_;
        std::string                             published_;
        std::string                             shell_owner_;
        std::string                             portal_owner_;
        std::string                             version_;
        bool                                    watch_portal_     = false;
        bool                                    extension_active_ = false;
        bool                                    drops_keys_       = false;
    };
#endif

} // namespace ngosen
