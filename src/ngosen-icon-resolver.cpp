/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "ngosen-icon-resolver.h"
#include <unistd.h>

namespace fcitx {

    static bool fileReadable(const std::string& path) {
        return !path.empty() && access(path.c_str(), R_OK) == 0;
    }

    static std::string joinPath(const std::string& dir, const std::string& name) {
        if (dir.empty())
            return name;
        if (dir.back() == '/')
            return dir + name;
        return dir + "/" + name;
    }

    std::string resolveNgoSenIconPath(const std::vector<std::string>& names, const NgoSenIconSearchPaths& paths) {
        // 1. Probe each candidate name in each system directory.
        //    SVG first (crisp at any size); PNG only as a raster fallback
        //    when the directory has no SVG.
        for (const auto& name : names) {
            for (const auto& dir : paths.systemDirs) {
                for (const char* ext : {".svg", ".png"}) {
                    const std::string p = joinPath(dir, name + ext);
                    if (fileReadable(p))
                        return p;
                }
            }
        }

        // 2. Fallback — the compile-time install directory.  Returned even if
        //    unreadable so the caller always gets a stable path.
        return joinPath(paths.fallbackDir, names.front() + ".svg");
    }

} // namespace fcitx
