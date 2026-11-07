/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-gnome-apps.h"

#include "ngosen-strings.h"

namespace ngosen {

    std::string focusedGnomeApp(const std::vector<RunningApp>& apps, bool overviewActive) {
        if (overviewActive)
            return gnomeOverviewApp;
        for (const RunningApp& app : apps) {
            if (app.activeOnSeat)
                return isStartsWith(app.id, "window:") ? std::string() : app.id;
        }
        return {};
    }

} // namespace ngosen
