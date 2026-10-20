// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file addon-conf.cpp
 * @brief Unit test: fcitx5 reads the dependencies declared in the generated addon files.
 *
 * fcitx5 looks for them under [Addon/Dependencies] and [Addon/OptionalDependencies]; any other
 * section name is ignored without a warning.
 */

#include <fcitx-config/iniparser.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx/addoninfo.h>

#include <algorithm>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    void load(fcitx::AddonInfo& info, const char* path) {
        // fcitx5 5.0 reads INI only from a file descriptor.
        const int fd = open(path, O_RDONLY);
        if (fd < 0) {
            std::cerr << "FAIL: cannot open " << path << '\n';
            ++failures;
            return;
        }
        fcitx::RawConfig config;
        fcitx::readFromIni(config, fd);
        close(fd);
        info.load(config);
    }

    bool hasCore(const fcitx::AddonInfo& info) {
        const auto& deps = info.dependenciesWithVersion();
        return std::any_of(deps.begin(), deps.end(), [](const auto& dep) { return std::get<0>(dep) == "core"; });
    }

    bool lists(const std::vector<std::string>& names, const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    }

} // namespace

int main() {
    fcitx::AddonInfo engine("ngosen");
    load(engine, NGOSEN_ADDON_CONF);
    check("the engine addon is valid", engine.isValid());
    check("the engine requires a minimum fcitx5", hasCore(engine));
    check("the engine can serve its D-Bus object", lists(engine.optionalDependencies(), "dbus"));

    fcitx::AddonInfo migration("ngosenmigrate");
    load(migration, NGOSEN_MIGRATE_CONF);
    check("the migration addon is valid", migration.isValid());
    check("the migration addon requires a minimum fcitx5", hasCore(migration));
    check("the migration addon loads after dbus", lists(migration.optionalDependencies(), "dbus"));
    check("the migration addon loads at startup", !migration.onDemand());

    if (failures == 0) {
        std::cout << "addon-conf: ok\n";
    }
    return failures == 0 ? 0 : 1;
}
