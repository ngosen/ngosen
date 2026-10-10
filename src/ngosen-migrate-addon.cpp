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
#if NGOSEN_USE_MODERN_FCITX_API
#include <fcitx-utils/standardpaths.h>
#else
#include <fcitx-utils/standardpath.h>
#endif

namespace {

    // Loaded at startup, before fcitx5 reads its profile, so the profile still lists the input
    // method under its old name and the user does not have to add Ngó Sen again.
    class NgoSenMigration : public fcitx::AddonInstance {
      public:
        NgoSenMigration() {
            copyUserFiles();
            renameInProfile();
        }

      private:
        static void copyUserFiles() {
#if NGOSEN_USE_MODERN_FCITX_API
            const auto& paths   = fcitx::StandardPaths::global();
            const auto  confDir = paths.userDirectory(fcitx::StandardPathsType::PkgConfig) / "conf";
            const auto  dataDir = paths.userDirectory(fcitx::StandardPathsType::PkgData);
#else
            const auto&                 paths = fcitx::StandardPath::global();
            const std::filesystem::path confDir(paths.userDirectory(fcitx::StandardPath::Type::PkgConfig) + "/conf");
            const std::filesystem::path dataDir(paths.userDirectory(fcitx::StandardPath::Type::PkgData));
#endif
            for (const auto& name : ngosen::copyLegacyFiles(confDir, ngosen::legacyConfigFiles())) {
                FCITX_INFO() << "Ngó Sen: copied settings to " << name;
            }
            ngosen::copyLegacyFiles(dataDir, {{"lotus/vietnamese.cm.dict", "ngosen/vietnamese.cm.dict"}});
        }

        static void renameInProfile() {
            // With fcitx5-lotus installed, "lotus" in the profile is that input method; leave it.
#if NGOSEN_USE_MODERN_FCITX_API
            if (!fcitx::StandardPaths::global().locate(fcitx::StandardPathsType::PkgData, "inputmethod/lotus.conf").empty()) {
                return;
            }
            const auto type = fcitx::StandardPathsType::PkgConfig;
#else
            if (!fcitx::StandardPath::global().locate(fcitx::StandardPath::Type::PkgData, "inputmethod/lotus.conf").empty()) {
                return;
            }
            const auto type = fcitx::StandardPath::Type::PkgConfig;
#endif
            fcitx::RawConfig profile;
            fcitx::readAsIni(profile, type, "profile");
            if (ngosen::renameProfileInputMethod(profile, ngosen::LegacyInputMethod, ngosen::InputMethod)) {
                fcitx::safeSaveAsIni(profile, type, "profile");
                FCITX_INFO() << "Ngó Sen: renamed the input method in the fcitx5 profile";
            }
        }
    };

    class NgoSenMigrationFactory : public fcitx::AddonFactory {
      public:
        fcitx::AddonInstance* create(fcitx::AddonManager* /*manager*/) override {
            return new NgoSenMigration;
        }
    };

} // namespace

FCITX_ADDON_FACTORY(NgoSenMigrationFactory)
