/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-program-name.h"

#include "ngosen-flatpak-names.h"

#include <fcitx-config/iniparser.h>
#include <fcitx-config/rawconfig.h>
#if NGOSEN_USE_MODERN_FCITX_API
#include <fcitx-utils/standardpaths.h>
#else
#include <fcitx-utils/standardpath.h>
#endif

#include <cstdio>
#include <unordered_map>
#include <vector>

namespace ngosen {

    namespace {
        std::vector<std::string> desktopFiles(const std::string& name) {
            const std::string        file = "applications/" + name + ".desktop";
            std::vector<std::string> found;
#if NGOSEN_USE_MODERN_FCITX_API
            for (const auto& path : fcitx::StandardPaths::global().locateAll(fcitx::StandardPathsType::Data, file))
                found.push_back(path.string());
#else
            found = fcitx::StandardPath::global().locateAll(fcitx::StandardPath::Type::Data, file);
#endif
            return found;
        }

        std::string resolve(const std::string& program) {
            const std::string appId = flatpakAppIdCandidate(program);
            if (appId.empty())
                return program;
            for (const auto& path : desktopFiles(appId)) {
                if (!isFlatpakExport(path))
                    continue;
                FILE* file = std::fopen(path.c_str(), "r");
                if (!file)
                    continue;
                fcitx::RawConfig entry;
                fcitx::readFromIni(entry, file);
                std::fclose(file);
                return programForFlatpakApp(appId, entry.valueByPath("Desktop Entry/StartupWMClass"));
            }
            return program;
        }
    } // namespace

    const std::string& normalizeProgramName(const std::string& program) {
        static std::unordered_map<std::string, std::string> cache;
        auto                                                it = cache.find(program);
        if (it == cache.end())
            it = cache.emplace(program, resolve(program)).first;
        return it->second;
    }

} // namespace ngosen
