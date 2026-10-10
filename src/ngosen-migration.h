/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef _FCITX5_NGOSEN_MIGRATION_H_
#define _FCITX5_NGOSEN_MIGRATION_H_

#include <fcitx-config/rawconfig.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ngosen {

    // Input method and file names used before the rename. fcitx5 drops a profile entry whose input
    // method no longer exists, so these must be rewritten before the profile is loaded.
    inline constexpr const char* LegacyInputMethod = "lotus";
    inline constexpr const char* InputMethod       = "ngosen";

    // Files in ~/.config/fcitx5/conf/, as {old, new}.
    std::vector<std::pair<std::string, std::string>> legacyConfigFiles();

    // Replaces input method `from` with `to` in an fcitx5 profile. A group that already lists `to`
    // drops `from` instead. Returns whether the profile changed.
    bool renameProfileInputMethod(fcitx::RawConfig& profile, const std::string& from, const std::string& to);

    // Copies dir/old to dir/new for each pair where only the old file exists. The old file stays, so
    // going back to an earlier version keeps its settings. Returns the new names that were written.
    std::vector<std::string> copyLegacyFiles(const std::filesystem::path& dir, const std::vector<std::pair<std::string, std::string>>& names);

} // namespace ngosen

#endif
