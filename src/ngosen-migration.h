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

    // Removes the keyboard from each group holding only `inputMethod` and one keyboard layout, so
    // fcitx5's switch key reaches Ngó Sen. The layout moves to the group's default layout if unset;
    // a group whose default layout is another one stays.
    bool dropLoneKeyboard(fcitx::RawConfig& profile, const std::string& inputMethod);

    // Makes the switch keys set in fcitx5's config Ngó Sen's toggle key, unless it has one. With one
    // input method left fcitx5 no longer switches. Returns whether `settings` changed.
    bool copySwitchKeys(const fcitx::RawConfig& fcitxConfig, fcitx::RawConfig& settings);

    // Settings saved before the toggle key existed, when fcitx5 switched to Ngó Sen with Ctrl+Space
    // and the saved mode said whether it typed Vietnamese, keep both. `saved` says the file existed.
    // Returns whether `settings` changed.
    bool upgradeSettings(fcitx::RawConfig& settings, bool saved);

    // The feature release (X.Y of `version`) to tell the user about, or nothing: a bug-fix release
    // and a fresh install are quiet. `saved` says the settings file existed. Updates `settings`.
    std::string releaseToAnnounce(fcitx::RawConfig& settings, bool saved, const std::string& version);

    // Copies dir/old to dir/new for each pair where only the old file exists. The old file stays, so
    // going back to an earlier version keeps its settings. Returns the new names that were written.
    std::vector<std::string> copyLegacyFiles(const std::filesystem::path& dir, const std::vector<std::pair<std::string, std::string>>& names);

    // PIDs under procDir (normally /proc) whose comm is `name` and that belong to `uid`, except `self`.
    std::vector<int> processesNamed(const std::filesystem::path& procDir, const std::string& name, unsigned uid, int self);

} // namespace ngosen

#endif
