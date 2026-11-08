/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ngosen {

    // Paths of the desktop files named appId + ".desktop" on the XDG data path, in lookup order.
    using DesktopFileFinder = std::function<std::vector<std::string>(const std::string& appId)>;
    // The file's StartupWMClass, trimmed as desktop-file parsers do ("" when it has none), or nullopt
    // when the file cannot be opened.
    using StartupWmClassReader = std::function<std::optional<std::string>(const std::string& path)>;

    // A Flatpak app reports its app ID (org.mozilla.firefox) where the distro package reports its
    // program name (firefox), which is what the app checks match. The shell only finds and reads
    // desktop files, so every shell maps names the same way. Unmapped names are returned as is.
    std::string flatpakProgramName(const std::string& program, const DesktopFileFinder& findDesktopFiles, const StartupWmClassReader& readStartupWmClass);

} // namespace ngosen
