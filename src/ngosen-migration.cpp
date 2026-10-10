/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-migration.h"

#include <system_error>

namespace ngosen {

    std::vector<std::pair<std::string, std::string>> legacyConfigFiles() {
        return {
            {"lotus.conf", "ngosen.conf"},
            {"lotus-custom-keymap.conf", "ngosen-custom-keymap.conf"},
            {"lotus-macro-table.conf", "ngosen-macro-table.conf"},
            {"lotus-app-rules.conf", "ngosen-app-rules.conf"},
            {"lotus-emoji-history.conf", "ngosen-emoji-history.conf"},
        };
    }

    namespace {
        struct ProfileItem {
            std::string name;
            std::string layout;
        };

        bool renameInGroup(fcitx::RawConfig& group, const std::string& from, const std::string& to) {
            bool changed = false;
            if (const auto* defaultIm = group.valueByPath("DefaultIM"); defaultIm && *defaultIm == from) {
                group.setValueByPath("DefaultIM", to);
                changed = true;
            }
            auto items = group.get("Items");
            if (!items) {
                return changed;
            }
            std::vector<ProfileItem> kept;
            bool                     hasTarget = false;
            bool                     hasSource = false;
            for (const auto& key : items->subItems()) {
                const auto* name   = items->valueByPath(key + "/Name");
                const auto* layout = items->valueByPath(key + "/Layout");
                ProfileItem item{name ? *name : "", layout ? *layout : ""};
                hasTarget = hasTarget || item.name == to;
                hasSource = hasSource || item.name == from;
                kept.push_back(std::move(item));
            }
            if (!hasSource) {
                return changed;
            }
            std::vector<ProfileItem> renamed;
            for (auto& item : kept) {
                if (item.name == from) {
                    if (hasTarget) {
                        continue;
                    }
                    item.name = to;
                }
                renamed.push_back(std::move(item));
            }
            items->removeAll();
            for (size_t i = 0; i < renamed.size(); ++i) {
                const auto key = std::to_string(i);
                items->setValueByPath(key + "/Name", renamed[i].name);
                items->setValueByPath(key + "/Layout", renamed[i].layout);
            }
            return true;
        }
    } // namespace

    bool renameProfileInputMethod(fcitx::RawConfig& profile, const std::string& from, const std::string& to) {
        auto groups = profile.get("Groups");
        if (!groups) {
            return false;
        }
        bool changed = false;
        for (const auto& key : groups->subItems()) {
            if (auto group = groups->get(key)) {
                changed = renameInGroup(*group, from, to) || changed;
            }
        }
        return changed;
    }

    std::vector<std::string> copyLegacyFiles(const std::filesystem::path& dir, const std::vector<std::pair<std::string, std::string>>& names) {
        std::vector<std::string> copied;
        for (const auto& [oldName, newName] : names) {
            std::error_code ec;
            const auto      oldPath = dir / oldName;
            const auto      newPath = dir / newName;
            if (!std::filesystem::is_regular_file(oldPath, ec) || std::filesystem::exists(newPath, ec)) {
                continue;
            }
            std::filesystem::create_directories(newPath.parent_path(), ec);
            if (std::filesystem::copy_file(oldPath, newPath, ec)) {
                copied.push_back(newName);
            }
        }
        return copied;
    }

} // namespace ngosen
