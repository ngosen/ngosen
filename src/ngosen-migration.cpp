/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-migration.h"

#include <fcitx-utils/stringutils.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <system_error>

#include <sys/stat.h>

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
        const std::string KeyboardPrefix = "keyboard-";

        struct ProfileItem {
            std::string name;
            std::string layout;
        };

        std::vector<ProfileItem> readItems(const fcitx::RawConfig& items) {
            std::vector<ProfileItem> list;
            for (const auto& key : items.subItems()) {
                const auto* name   = items.valueByPath(key + "/Name");
                const auto* layout = items.valueByPath(key + "/Layout");
                list.push_back({name ? *name : "", layout ? *layout : ""});
            }
            return list;
        }

        void writeItems(fcitx::RawConfig& items, const std::vector<ProfileItem>& list) {
            items.removeAll();
            for (size_t i = 0; i < list.size(); ++i) {
                const auto key = std::to_string(i);
                items.setValueByPath(key + "/Name", list[i].name);
                items.setValueByPath(key + "/Layout", list[i].layout);
            }
        }

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
            auto kept      = readItems(*items);
            bool hasTarget = false;
            bool hasSource = false;
            for (const auto& item : kept) {
                hasTarget = hasTarget || item.name == to;
                hasSource = hasSource || item.name == from;
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
            writeItems(*items, renamed);
            return true;
        }

        bool dropKeyboardInGroup(fcitx::RawConfig& group, const std::string& inputMethod) {
            auto items = group.get("Items");
            if (!items) {
                return false;
            }
            const auto         list     = readItems(*items);
            const ProfileItem* own      = nullptr;
            const ProfileItem* keyboard = nullptr;
            for (const auto& item : list) {
                if (item.name == inputMethod && !own) {
                    own = &item;
                } else if (fcitx::stringutils::startsWith(item.name, KeyboardPrefix) && !keyboard) {
                    keyboard = &item;
                } else {
                    // A second layout or another input method needs fcitx5's switch key.
                    return false;
                }
            }
            if (!own || !keyboard) {
                return false;
            }
            const auto keyboardLayout = keyboard->name.substr(KeyboardPrefix.size());
            if (const auto* layout = group.valueByPath("Default Layout"); !layout || layout->empty()) {
                group.setValueByPath("Default Layout", keyboardLayout);
            } else if (*layout != keyboardLayout) {
                // English was typed on the keyboard's layout; Ngó Sen alone would type it on this one.
                return false;
            }
            group.setValueByPath("DefaultIM", inputMethod);
            writeItems(*items, {*own});
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

    bool dropLoneKeyboard(fcitx::RawConfig& profile, const std::string& inputMethod) {
        auto groups = profile.get("Groups");
        if (!groups) {
            return false;
        }
        bool changed = false;
        for (const auto& key : groups->subItems()) {
            if (auto group = groups->get(key)) {
                changed = dropKeyboardInGroup(*group, inputMethod) || changed;
            }
        }
        return changed;
    }

    bool upgradeSettings(fcitx::RawConfig& settings, bool saved) {
        constexpr int Version = 1;
        if (const auto* version = settings.valueByPath("SettingsVersion"); version != nullptr && std::atoi(version->c_str()) >= Version)
            return false;
        if (saved) {
            if (settings.get("ToggleKey") == nullptr)
                settings.setValueByPath("ToggleKey/0", "Control+space");
            if (settings.valueByPath("VietnameseOn") == nullptr) {
                const auto* mode = settings.valueByPath("Mode");
                settings.setValueByPath("VietnameseOn", mode != nullptr && *mode == "OFF" ? "False" : "True");
            }
            // Retyping was a hidden option, so a saved False is fcitx5's default, not the user's choice.
            settings.setValueByPath("RetypeAfterSwitch", "True");
        }
        settings.setValueByPath("SettingsVersion", std::to_string(Version));
        return true;
    }

    // "1.6.1" reads as {1, 6}: a bug-fix release shares its feature release's number.
    static std::pair<int, int> featureRelease(const std::string& version) {
        int major = 0;
        int minor = 0;
        std::sscanf(version.c_str(), "%d.%d", &major, &minor);
        return {major, minor};
    }

    std::string releaseToAnnounce(fcitx::RawConfig& settings, bool saved, const std::string& version) {
        const auto        release = featureRelease(version);
        const std::string current = std::to_string(release.first) + "." + std::to_string(release.second);
        if (const auto* announced = settings.valueByPath("AnnouncedVersion"); announced != nullptr && featureRelease(*announced) >= release)
            return {};
        settings.setValueByPath("AnnouncedVersion", current);
        // A fresh install has nothing to catch up on.
        return saved ? current : std::string();
    }

    bool copySwitchKeys(const fcitx::RawConfig& fcitxConfig, fcitx::RawConfig& settings) {
        const auto keys = fcitxConfig.get("Hotkey/TriggerKeys");
        if (!keys || keys->subItemsSize() == 0 || settings.get("ToggleKey")) {
            return false;
        }
        for (const auto& index : keys->subItems()) {
            if (const auto* key = keys->valueByPath(index)) {
                settings.setValueByPath("ToggleKey/" + index, *key);
            }
        }
        return true;
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

    std::vector<int> processesNamed(const std::filesystem::path& procDir, const std::string& name, unsigned uid, int self) {
        std::vector<int> pids;
        std::error_code  ec;
        for (const auto& entry : std::filesystem::directory_iterator(procDir, ec)) {
            const auto file = entry.path().filename().string();
            if (file.empty() || file.size() > 9 || file.find_first_not_of("0123456789") != std::string::npos) {
                continue;
            }
            const int pid = std::stoi(file);
            if (pid == self) {
                continue;
            }
            struct stat info{};
            if (stat(entry.path().c_str(), &info) != 0 || info.st_uid != uid) {
                continue;
            }
            std::ifstream in(entry.path() / "comm");
            std::string   comm;
            if (std::getline(in, comm) && comm == name) {
                pids.push_back(pid);
            }
        }
        return pids;
    }

} // namespace ngosen
