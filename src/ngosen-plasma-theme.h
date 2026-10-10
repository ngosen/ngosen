/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file ngosen-plasma-theme.h
 * @brief Detects whether the KDE Plasma panel is dark.
 *
 * KDE keeps two independent colour settings: the application colour scheme
 * (kdeglobals) and the Plasma Style that paints the panel (plasmarc).  The
 * settings portal only reports the former, so the default Fedora/Kubuntu
 * look — light applications on a dark panel — picked a black tray icon on a
 * black panel (issue #374).  The tray lives on the panel, so the panel's
 * colours are the ones that matter.
 *
 * Only plain config/data files are read; no subprocess is spawned.
 */

#ifndef _FCITX5_NGOSEN_PLASMA_THEME_H_
#define _FCITX5_NGOSEN_PLASMA_THEME_H_

#include <optional>
#include <string>
#include <vector>

namespace fcitx {

    struct PlasmaThemeSearchPaths {
        // Config directories searched in KConfig cascade order: the user
        // directory first, then each system directory.
        std::string              configHome;
        std::vector<std::string> configDirs;
        // Data directories holding plasma/desktoptheme and color-schemes.
        std::string              dataHome;
        std::vector<std::string> dataDirs;
    };

    /**
     * @brief Builds search paths from the XDG environment variables.
     *
     * Adds "<configHome>/kdedefaults" (where Plasma 6 stores the Global
     * Theme defaults) when the environment does not list it already.
     */
    PlasmaThemeSearchPaths plasmaThemeSearchPathsFromEnv();

    /**
     * @brief Tells whether XDG_CURRENT_DESKTOP names KDE.
     */
    bool isKdePlasmaSession(const std::string& xdgCurrentDesktop);

    /**
     * @brief Reports whether the Plasma panel background is dark.
     *
     * Uses the Plasma Style's own colours when it ships a colors file, and
     * the system colour scheme otherwise (Breeze "default" follows it).
     *
     * @return true for a dark panel, false for a light one, std::nullopt
     *         when no colour could be found.
     */
    std::optional<bool> isPlasmaPanelDark(const PlasmaThemeSearchPaths& paths);

} // namespace fcitx

#endif // _FCITX5_NGOSEN_PLASMA_THEME_H_
