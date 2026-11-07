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
        std::string lowercaseWithoutSpaces(std::string text) {
            text.erase(std::remove(text.begin(), text.end(), ' '), text.end());
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool looksLikeAppId(const std::string& name) {
            return std::count(name.begin(), name.end(), '.') >= 2 && name.find('/') == std::string::npos;
        }
    } // namespace

    std::string flatpakAppIdCandidate(const std::string& program) {
        std::string name = stripDesktopSuffix(program);
        return looksLikeAppId(name) ? name : std::string();
    }

    bool isFlatpakExport(const std::string& desktopFilePath) {
        return desktopFilePath.find("/flatpak/exports/share/applications/") != std::string::npos;
    }

    std::string programForFlatpakApp(const std::string& appId, const std::string* startupWmClass) {
        // Mullvad Browser's class is "Mullvad Browser", Lutris gives its app ID.
        if (startupWmClass && !startupWmClass->empty() && !looksLikeAppId(*startupWmClass))
            return lowercaseWithoutSpaces(*startupWmClass);
        return lowercaseWithoutSpaces(appId.substr(appId.rfind('.') + 1));
    }

} // namespace ngosen
