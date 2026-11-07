/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-flatpak-names.h"

#include "ngosen-strings.h"

#include <algorithm>
#include <cctype>

namespace ngosen {

    namespace {
        // Desktop-file parsers differ in what whitespace they trim, so none of it may reach the name.
        std::string lowercaseWithoutSpaces(std::string text) {
            text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); }), text.end());
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool looksLikeAppId(const std::string& name) {
            return std::count(name.begin(), name.end(), '.') >= 2 && name.find('/') == std::string::npos;
        }

        bool isFlatpakExport(const std::string& path) {
            return path.find("/flatpak/exports/share/applications/") != std::string::npos;
        }

        std::string programForAppId(const std::string& appId, const std::string& startupWmClass) {
            // Mullvad Browser's class is "Mullvad Browser", Lutris gives its app ID.
            const std::string wmClass = lowercaseWithoutSpaces(startupWmClass);
            if (!wmClass.empty() && !looksLikeAppId(wmClass))
                return wmClass;
            return lowercaseWithoutSpaces(appId.substr(appId.rfind('.') + 1));
        }
    } // namespace

    std::string flatpakProgramName(const std::string& program, const DesktopFileFinder& findDesktopFiles, const StartupWmClassReader& readStartupWmClass) {
        const std::string appId = stripDesktopSuffix(program);
        if (!looksLikeAppId(appId))
            return program;
        for (const std::string& path : findDesktopFiles(appId)) {
            // A native app may use a reverse-DNS name too; only a Flatpak export proves the app ID.
            if (!isFlatpakExport(path))
                continue;
            if (const auto wmClass = readStartupWmClass(path))
                return programForAppId(appId, *wmClass);
        }
        return program;
    }

} // namespace ngosen
