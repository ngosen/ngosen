/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ngosen {

    // The name the GNOME Shell extension and fcitx5 give the overview's search box.
    inline constexpr const char* gnomeOverviewApp = "gnome-shell-overview";

    // One entry of org.gnome.Shell.Introspect.GetRunningApplications.
    struct RunningApp {
        std::string id;                   // GNOME's app id, such as "org.gnome.Nautilus.desktop"
        bool        activeOnSeat = false; // the entry carries "active-on-seats"
    };

    // The app being typed into, named as GNOME names it so rules saved under that name still match.
    // overviewActive is GNOME Shell's OverviewActive property, which the reply does not carry. Empty
    // when no listed app has the focus or the focused window has no .desktop file ("window:NNN").
    std::string focusedGnomeApp(const std::vector<RunningApp>& apps, bool overviewActive);

    // One entry of a GetRunningApplications reply as read off the bus. activeOnSeats is empty when
    // the entry has no such property or it is not a list of strings.
    struct IntrospectEntry {
        std::string              id;
        std::vector<std::string> activeOnSeats;
    };
    std::vector<RunningApp> runningApps(const std::vector<IntrospectEntry>& reply);

    // Pairs the portal's GetRunningApplications calls with GNOME Shell's replies, as a bus monitor
    // sees them. Senders and destinations are unique bus names.
    class IntrospectReplyMatcher {
      public:
        // A new owner of either name makes every call seen before it stale.
        void setOwners(const std::string& shell, const std::string& portal);
        // serial is 0 when the bus library does not tell it.
        void call(const std::string& sender, const std::string& destination, uint64_t serial);
        // Whether a reply answers the newest call. Without serials, any reply of the right type from
        // GNOME Shell to the portal counts: D-Bus keeps one sender's messages in order.
        bool answers(const std::string& sender, const std::string& destination, uint64_t replySerial, const std::string& signature);

      private:
        std::string shell_;
        std::string portal_;
        bool        called_  = false;
        uint64_t    pending_ = 0;
    };

    // The focused app from what GNOME Shell tells: the extension's name while it runs, else the
    // portal's, with the search box while the overview is shown.
    class GnomeAppNames {
      public:
        // Each returns whether name() changed.
        bool               setExtensionRunning(bool running);
        bool               setExtensionApp(const std::string& app);
        bool               setPortalApps(const std::vector<RunningApp>& apps);
        bool               dropPortalApp();
        bool               setOverviewActive(bool active);

        const std::string& name() const {
            return name_;
        }
        // Where name() comes from, for the log.
        const char* source() const;
        // Whether the portal has asked since GNOME Shell or the portal last started, even about a window with no name.
        bool portalAnswered() const {
            return portal_answered_;
        }

      private:
        bool        update();
        bool        usesExtension() const;

        bool        extension_running_ = false;
        std::string extension_app_;
        // Only the focused entry is kept: the whole list tells which other apps run.
        std::vector<RunningApp> portal_focus_;
        bool                    portal_answered_ = false;
        bool                    overview_        = false;
        std::string             name_;
    };

} // namespace ngosen
