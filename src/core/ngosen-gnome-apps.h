/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

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

} // namespace ngosen
