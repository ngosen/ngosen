/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

namespace ngosen {

    // A Flatpak app reports its app ID (org.mozilla.firefox) where the distro package reports its
    // program name (firefox), which is what the app checks match. The shell finds the desktop file;
    // these functions hold the rule, so every shell maps names the same way.

    // The app ID to look up a desktop file for, or empty when the name cannot be one.
    std::string flatpakAppIdCandidate(const std::string& program);

    // A native app may use a reverse-DNS name too; only a Flatpak export proves the app ID.
    bool isFlatpakExport(const std::string& desktopFilePath);

    // startupWmClass is the desktop file's StartupWMClass, or nullptr when it has none.
    std::string programForFlatpakApp(const std::string& appId, const std::string* startupWmClass);

} // namespace ngosen
