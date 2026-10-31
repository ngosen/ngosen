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

#include <memory>
#endif

namespace ngosen {

    // GNOME Shell 50.0 to 50.3 lose the keys an input method forwards (mutter#4853).
    bool gnomeShellDropsForwardedKeys(const std::string& version);

#ifdef NGOSEN_HAS_DBUS
    // Whether GNOME Shell loses forwarded keys right now: a dropping version without forward-keys@ngosen.github.io running.
    class GnomeShellWatcher {
      public:
        explicit GnomeShellWatcher(fcitx::dbus::Bus* bus);

        const bool* dropsKeys() const {
            return &drops_keys_;
        }

      private:
        void                                              refresh();
        void                                              update();

        fcitx::dbus::Bus*                                 bus_;
        fcitx::dbus::ServiceWatcher                       watcher_;
        std::unique_ptr<fcitx::dbus::ServiceWatcherEntry> owner_entry_;
        std::unique_ptr<fcitx::dbus::Slot>                state_signal_;
        std::unique_ptr<fcitx::dbus::Slot>                version_call_;
        std::unique_ptr<fcitx::dbus::Slot>                extension_call_;
        std::string                                       version_;
        bool                                              extension_active_ = false;
        bool                                              drops_keys_       = false;
    };
#endif

} // namespace ngosen
