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

    std::vector<RunningApp> runningApps(const std::vector<IntrospectEntry>& reply) {
        std::vector<RunningApp> apps;
        apps.reserve(reply.size());
        for (const IntrospectEntry& entry : reply)
            apps.push_back({entry.id, !entry.activeOnSeats.empty()});
        return apps;
    }

    void IntrospectReplyMatcher::setOwners(const std::string& shell, const std::string& portal) {
        if (shell == shell_ && portal == portal_)
            return;
        shell_   = shell;
        portal_  = portal;
        called_  = false;
        pending_ = 0;
    }

    void IntrospectReplyMatcher::call(const std::string& sender, const std::string& destination, uint64_t serial) {
        // The bus hands a monitor calls to the well-known name too, and the header keeps that name.
        if (portal_.empty() || sender != portal_ || (destination != shell_ && destination != "org.gnome.Shell"))
            return;
        called_  = true;
        pending_ = serial;
    }

    bool IntrospectReplyMatcher::answers(const std::string& sender, const std::string& destination, uint64_t replySerial, const std::string& signature) {
        if (!called_ || shell_.empty() || sender != shell_ || destination != portal_ || signature != "a{sa{sv}}")
            return false;
        if (pending_ == 0)
            return true;
        if (replySerial != pending_)
            return false;
        called_  = false;
        pending_ = 0;
        return true;
    }

    bool GnomeAppNames::setExtensionRunning(bool running) {
        extension_running_ = running;
        if (!running)
            extension_app_.clear();
        return update();
    }

    bool GnomeAppNames::setExtensionApp(const std::string& app) {
        extension_running_ = true;
        extension_app_     = app;
        return update();
    }

    bool GnomeAppNames::setPortalApps(const std::vector<RunningApp>& apps) {
        // In the overview the app changes both when a window is picked and when one is closed, and
        // the search box keeps the focus after a close, so the overview lasts until OverviewActive ends.
        const std::string app = focusedGnomeApp(apps, false);
        portal_answered_      = true;
        portal_focus_.clear();
        if (!app.empty())
            portal_focus_.push_back({app, true});
        return update();
    }

    bool GnomeAppNames::dropPortalApp() {
        portal_answered_ = false;
        portal_focus_.clear();
        return update();
    }

    bool GnomeAppNames::setOverviewActive(bool active) {
        overview_ = active;
        return update();
    }

    bool GnomeAppNames::usesExtension() const {
        // The extension also names windows without a .desktop file, and the overview itself.
        return extension_running_ && !extension_app_.empty();
    }

    const char* GnomeAppNames::source() const {
        if (usesExtension())
            return "the extension";
        if (overview_)
            return "OverviewActive";
        return portal_focus_.empty() ? "nothing" : "the portal";
    }

    bool GnomeAppNames::update() {
        std::string name = usesExtension() ? extension_app_ : focusedGnomeApp(portal_focus_, overview_);
        if (name == name_)
            return false;
        name_ = std::move(name);
        return true;
    }

} // namespace ngosen
