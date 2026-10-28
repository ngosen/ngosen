/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-migration.h"

#include <fcitx-config/iniparser.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/instance.h>
#if NGOSEN_USE_MODERN_FCITX_API
#include <fcitx-utils/standardpaths.h>
#else
#include <fcitx-utils/standardpath.h>
#endif

#include <chrono>
#include <thread>

#include <unistd.h>

namespace {

    // Loaded at startup, before fcitx5 reads its profile, so the profile still lists the input
    // method under its old name and the user does not have to add Ngó Sen again.
    class NgoSenMigration : public fcitx::AddonInstance {
      public:
        explicit NgoSenMigration(fcitx::Instance* instance) {
            copyUserFiles();
            updateProfile(instance->willTryReplace());
        }

      private:
        static std::filesystem::path confDir() {
#if NGOSEN_USE_MODERN_FCITX_API
            return fcitx::StandardPaths::global().userDirectory(fcitx::StandardPathsType::PkgConfig) / "conf";
#else
            return fcitx::StandardPath::global().userDirectory(fcitx::StandardPath::Type::PkgConfig) + "/conf";
#endif
        }

#if NGOSEN_USE_MODERN_FCITX_API
        static constexpr auto profileType() {
            return fcitx::StandardPathsType::PkgConfig;
        }
#else
        static constexpr auto profileType() {
            return fcitx::StandardPath::Type::PkgConfig;
        }
#endif

        static void copyUserFiles() {
#if NGOSEN_USE_MODERN_FCITX_API
            const auto dataDir = fcitx::StandardPaths::global().userDirectory(fcitx::StandardPathsType::PkgData);
#else
            const std::filesystem::path dataDir(fcitx::StandardPath::global().userDirectory(fcitx::StandardPath::Type::PkgData));
#endif
            for (const auto& name : ngosen::copyLegacyFiles(confDir(), ngosen::legacyConfigFiles())) {
                FCITX_INFO() << "Ngó Sen: copied settings to " << name;
            }
            ngosen::copyLegacyFiles(dataDir, {{"lotus/vietnamese.cm.dict", "ngosen/vietnamese.cm.dict"}});
        }

        // `fcitx5 -r` takes over from the running fcitx5, which then saves its profile, still under
        // the old name, as it exits. That happens after this addon runs (it loads after dbus, which
        // asks the old one to quit), so wait for it or it overwrites the rename.
        static bool waitForPreviousInstance() {
            using namespace std::chrono_literals;
            const auto self   = static_cast<int>(getpid());
            bool       waited = false;
            for (auto deadline = std::chrono::steady_clock::now() + 3s; std::chrono::steady_clock::now() < deadline;) {
                if (ngosen::processesNamed("/proc", "fcitx5", getuid(), self).empty()) {
                    return waited;
                }
                waited = true;
                std::this_thread::sleep_for(50ms);
            }
            FCITX_WARN() << "Ngó Sen: the previous fcitx5 is still running";
            return waited;
        }

        // The engine loads only once switched to, so a new user who adds Ngó Sen in the configtool
        // has no settings yet on the next start. Users who upgrade already list it.
        static bool markNewAccount(bool replacing) {
            if (std::filesystem::exists(confDir() / "ngosen.conf")) {
                return false;
            }
            if (replacing) {
                waitForPreviousInstance();
            }
            fcitx::RawConfig profile;
            fcitx::readAsIni(profile, profileType(), "profile");
            if (ngosen::listsInputMethod(profile, ngosen::InputMethod) || ngosen::listsInputMethod(profile, ngosen::LegacyInputMethod)) {
                return false;
            }
            fcitx::RawConfig settings;
            ngosen::upgradeSettings(settings, false);
            fcitx::safeSaveAsIni(settings, profileType(), "conf/ngosen.conf");
            return true;
        }

        struct ProfileChanges {
            bool renamed = false;
            bool dropped = false;
        };

        static void updateProfile(bool replacing) {
#if NGOSEN_USE_MODERN_FCITX_API
            const auto type = fcitx::StandardPathsType::PkgConfig;
            // With fcitx5-lotus installed, "lotus" in the profile is that input method; leave it.
            const bool rename = fcitx::StandardPaths::global().locate(fcitx::StandardPathsType::PkgData, "inputmethod/lotus.conf").empty();
#else
            const auto type   = fcitx::StandardPath::Type::PkgConfig;
            const bool rename = fcitx::StandardPath::global().locate(fcitx::StandardPath::Type::PkgData, "inputmethod/lotus.conf").empty();
#endif
            // Settings not upgraded yet, or none at all for users who never changed one, are from
            // before Ngó Sen had its own toggle key, when users reached it through fcitx5's switch
            // from the keyboard. Only then, so a keyboard the user adds back stays.
            fcitx::RawConfig settings;
            fcitx::readAsIni(settings, type, "conf/ngosen.conf");
            const bool dropKeyboard = settings.valueByPath("SettingsVersion") == nullptr;

            const auto readChanged = [=](fcitx::RawConfig& profile) {
                fcitx::readAsIni(profile, type, "profile");
                ProfileChanges changes;
                changes.renamed = rename && ngosen::renameProfileInputMethod(profile, ngosen::LegacyInputMethod, ngosen::InputMethod);
                changes.dropped = dropKeyboard && ngosen::dropLoneKeyboard(profile, ngosen::InputMethod);
                return changes;
            };
            if (dropKeyboard && markNewAccount(replacing)) {
                return;
            }
            fcitx::RawConfig profile;
            auto             changes = readChanged(profile);
            if (!changes.renamed && !changes.dropped) {
                return;
            }
            if (replacing && waitForPreviousInstance()) {
                // Start again from the profile it saved on the way out.
                fcitx::RawConfig saved;
                changes = readChanged(saved);
                if (!changes.renamed && !changes.dropped) {
                    return;
                }
                fcitx::safeSaveAsIni(saved, type, "profile");
            } else {
                fcitx::safeSaveAsIni(profile, type, "profile");
            }
            if (changes.renamed) {
                FCITX_INFO() << "Ngó Sen: renamed the input method in the fcitx5 profile";
            }
            if (changes.dropped) {
                FCITX_INFO() << "Ngó Sen: removed the keyboard next to Ngó Sen from the fcitx5 profile";
                fcitx::RawConfig fcitxConfig;
                fcitx::readAsIni(fcitxConfig, type, "config");
                // Read again: a previous fcitx5 may have saved it on the way out.
                fcitx::RawConfig current;
                fcitx::readAsIni(current, type, "conf/ngosen.conf");
                ngosen::copySwitchKeys(fcitxConfig, current);
                // Now, not when the engine first loads, or the next start drops a keyboard added back.
                ngosen::upgradeSettings(current, true);
                fcitx::safeSaveAsIni(current, type, "conf/ngosen.conf");
            }
        }
    };

    class NgoSenMigrationFactory : public fcitx::AddonFactory {
      public:
        fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
            return new NgoSenMigration(manager->instance());
        }
    };

} // namespace

FCITX_ADDON_FACTORY(NgoSenMigrationFactory)
