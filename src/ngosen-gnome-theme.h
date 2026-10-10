/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file ngosen-gnome-theme.h
 * @brief Detects whether the GNOME Shell top bar is dark.
 *
 * Tray icons sit on the top bar, which the shell stylesheet paints, while the
 * settings portal reports the app colour scheme; the two often disagree (Yaru
 * paints a dark bar under the light scheme). See #374 for the KDE case.
 *
 * The stylesheet is found the way GNOME Shell does it: the User Themes
 * extension first, then the session mode's own stylesheet.
 */

#ifndef _FCITX5_NGOSEN_GNOME_THEME_H_
#define _FCITX5_NGOSEN_GNOME_THEME_H_

#include <optional>
#include <string>
#include <vector>

namespace fcitx {

    struct GnomeShellThemeInfo {
        // Name set in the User Themes extension; empty when the extension is
        // not enabled or no theme is chosen.
        std::string userThemeName;
        // GNOME_SHELL_SESSION_MODE, e.g. "ubuntu".  Empty means "user".
        std::string              sessionMode;
        std::string              home;
        std::string              dataHome;
        std::vector<std::string> dataDirs;
    };

    /**
     * @brief Builds the lookup inputs from the environment and gsettings.
     */
    GnomeShellThemeInfo gnomeShellThemeInfoFromEnv();

    /**
     * @brief Tells whether XDG_CURRENT_DESKTOP names a GNOME Shell session.
     *
     * Desktops that only borrow the GNOME token (Budgie, Pantheon,
     * GNOME Flashback, ...) draw their own panel and are rejected.
     */
    bool isGnomeShellSession(const std::string& xdgCurrentDesktop);

    /**
     * @brief Reads the "#panel" rule of a GNOME Shell stylesheet.
     *
     * The text colour wins when present, because translucent bars take their
     * background from the wallpaper and the theme picks the text colour to
     * match.  Otherwise an opaque background colour decides.
     *
     * @return true for a dark bar, false for a light one, std::nullopt when
     *         the rule says neither.
     */
    std::optional<bool> isShellCssPanelDark(const std::string& css);

    /**
     * @brief Path of the stylesheet GNOME Shell paints the top bar with.
     */
    std::optional<std::string> gnomeShellStylesheet(const GnomeShellThemeInfo& info);

    /**
     * @brief Reports whether the GNOME Shell top bar is dark.
     *
     * @return std::nullopt when no stylesheet is readable, e.g. the built-in
     *         Adwaita theme compiled into a GResource.
     */
    std::optional<bool> isGnomePanelDark(const GnomeShellThemeInfo& info);

} // namespace fcitx

#endif // _FCITX5_NGOSEN_GNOME_THEME_H_
