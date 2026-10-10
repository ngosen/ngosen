/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file ngosen-icon-resolver.h
 * @brief Absolute-path resolution for Ngó Sen tray/status icons.
 *
 * The fcitx5 SNI tray host resolves the engine icon through the active
 * XDG icon theme.  Themes other than Breeze (or themes not inheriting
 * hicolor) fail that lookup even though the icons are installed, so the
 * tray shows no icon.  Following the approach used by fcitx5-skey, the
 * engine instead resolves an absolute filesystem path and returns it from
 * subModeIconImpl(); DE compositors accept paths in the SNI IconName
 * property, bypassing the theme lookup entirely.
 */

#ifndef _FCITX5_NGOSEN_ICON_RESOLVER_H_
#define _FCITX5_NGOSEN_ICON_RESOLVER_H_

#include <string>
#include <vector>

namespace fcitx {

    struct NgoSenIconSearchPaths {
        // System install directories, searched in order.  Each is probed as
        // "<dir>/<name>.svg" then "<dir>/<name>.png" (SVG first; PNG only as
        // a raster fallback).
        std::vector<std::string> systemDirs;
        // Fallback directory — the compile-time install dir, always present.
        // Used as "<fallbackDir>/<first name>.svg" when nothing else exists.
        std::string fallbackDir;
    };

    /**
     * @brief Resolves the absolute path of a Ngó Sen icon.
     *
     * Probes each candidate name (variant first, plain mode icon second) in
     * every system directory, SVG before PNG.  Returns the fallback path if
     * nothing readable is found.
     *
     * @param names Candidate icon base names, in priority order.
     * @param paths Search paths.
     * @return Absolute path to the resolved icon.
     */
    std::string resolveNgoSenIconPath(const std::vector<std::string>& names, const NgoSenIconSearchPaths& paths);

} // namespace fcitx

#endif // _FCITX5_NGOSEN_ICON_RESOLVER_H_
