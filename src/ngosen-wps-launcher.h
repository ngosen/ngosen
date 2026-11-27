/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef _FCITX5_NGOSEN_WPS_LAUNCHER_H_
#define _FCITX5_NGOSEN_WPS_LAUNCHER_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ngosen {

    // WPS bundles its own Qt 5, which has neither KDE's Wayland input protocol nor XIM, so on Plasma
    // Wayland it types through fcitx5 only when started with QT_IM_MODULE=fcitx.
    bool wantsWpsInputMethod(const std::string& currentDesktop, const std::string& sessionType);

    // The launcher `entry` starting WPS with fcitx5's Qt module, or nothing when it has no Exec line.
    std::optional<std::string> wpsLauncherWithInputMethod(const std::string& entry);

    // Writes such a launcher into `userDir` for each wps-office-*.desktop in `systemDirs` (earlier
    // directories win) that has no launcher of that name there yet. Returns the names written.
    std::vector<std::string> addWpsLaunchers(const std::vector<std::filesystem::path>& systemDirs, const std::filesystem::path& userDir);

    // A Flatpak override file with QT_IM_MODULE=fcitx added, or nothing when it already sets one.
    std::optional<std::string> flatpakOverrideWithInputMethod(const std::string& overrides);

} // namespace ngosen

#endif
