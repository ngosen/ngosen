// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ngosen-migration.cpp
 * @brief Unit test: settings saved under the lotus names carry over to the ngosen names.
 *
 * fcitx5 drops a profile entry whose input method does not exist, so a user upgrading from a
 * version that registered "lotus" would lose Ngó Sen from the input method list.
 */

#include "ngosen-migration.h"

#include <fcitx-config/iniparser.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    std::string value(const fcitx::RawConfig& config, const std::string& path) {
        const auto* v = config.valueByPath(path);
        return v ? *v : "<missing>";
    }

    // The profile as fcitx5 writes it. fcitx5 5.0 reads INI only from a file descriptor.
    fcitx::RawConfig parse(const std::string& ini) {
        char tmpl[] = "/tmp/ngosen-migration-profile-XXXXXX";
        int  fd     = mkstemp(tmpl);
        if (fd < 0 || write(fd, ini.data(), ini.size()) != static_cast<ssize_t>(ini.size()) || lseek(fd, 0, SEEK_SET) != 0) {
            std::cerr << "FAIL: cannot write a temporary profile\n";
            std::exit(1);
        }
        fcitx::RawConfig config;
        fcitx::readFromIni(config, fd);
        close(fd);
        unlink(tmpl);
        return config;
    }

    std::string read(const std::filesystem::path& path) {
        std::ifstream     in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    void testRename() {
        auto profile = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=lotus\n\n"
                             "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
                             "[Groups/0/Items/1]\nName=lotus\nLayout=us\n\n"
                             "[GroupOrder]\n0=Default\n");
        check("a profile listing lotus changes", ngosen::renameProfileInputMethod(profile, "lotus", "ngosen"));
        check("the default input method is renamed", value(profile, "Groups/0/DefaultIM") == "ngosen");
        check("the first item stays", value(profile, "Groups/0/Items/0/Name") == "keyboard-us");
        check("the lotus item is renamed", value(profile, "Groups/0/Items/1/Name") == "ngosen");
        check("its layout is kept", value(profile, "Groups/0/Items/1/Layout") == "us");
        check("the group name is kept", value(profile, "Groups/0/Name") == "Default");
        check("the group order is kept", value(profile, "GroupOrder/0") == "Default");
        check("a second pass changes nothing", !ngosen::renameProfileInputMethod(profile, "lotus", "ngosen"));
    }

    void testBothListed() {
        auto profile = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n"
                             "[Groups/0/Items/0]\nName=lotus\nLayout=\n\n"
                             "[Groups/0/Items/1]\nName=keyboard-us\nLayout=\n\n"
                             "[Groups/0/Items/2]\nName=ngosen\nLayout=\n\n"
                             "[GroupOrder]\n0=Default\n");
        check("a group listing both changes", ngosen::renameProfileInputMethod(profile, "lotus", "ngosen"));
        check("the lotus item is dropped", value(profile, "Groups/0/Items/0/Name") == "keyboard-us");
        check("the ngosen item moves up", value(profile, "Groups/0/Items/1/Name") == "ngosen");
        check("no third item is left", !profile.get("Groups/0/Items/2"));
        check("another default input method is kept", value(profile, "Groups/0/DefaultIM") == "keyboard-us");
    }

    void testOtherGroups() {
        auto profile = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n"
                             "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
                             "[Groups/1]\nName=Viet\nDefault Layout=us\nDefaultIM=lotus\n\n"
                             "[Groups/1/Items/0]\nName=lotus\nLayout=\n\n"
                             "[GroupOrder]\n0=Default\n1=Viet\n");
        check("a second group is renamed", ngosen::renameProfileInputMethod(profile, "lotus", "ngosen"));
        check("its item is renamed", value(profile, "Groups/1/Items/0/Name") == "ngosen");
        check("the first group is untouched", value(profile, "Groups/0/Items/0/Name") == "keyboard-us");

        fcitx::RawConfig empty;
        check("an empty profile changes nothing", !ngosen::renameProfileInputMethod(empty, "lotus", "ngosen"));
    }

    void testCopyFiles() {
        const auto dir = std::filesystem::temp_directory_path() / "ngosen-migration-files";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir / "lotus");
        std::ofstream(dir / "lotus.conf") << "Mode=Sen\n";
        std::ofstream(dir / "lotus-macro-table.conf") << "old macros\n";
        std::ofstream(dir / "ngosen-macro-table.conf") << "new macros\n";
        std::ofstream(dir / "lotus/vietnamese.cm.dict") << "từ\n";

        const auto copied = ngosen::copyLegacyFiles(dir,
                                                    {{"lotus.conf", "ngosen.conf"},
                                                     {"lotus-macro-table.conf", "ngosen-macro-table.conf"},
                                                     {"lotus-app-rules.conf", "ngosen-app-rules.conf"},
                                                     {"lotus/vietnamese.cm.dict", "ngosen/vietnamese.cm.dict"}});
        check("two files are reported copied", copied.size() == 2);
        check("lotus.conf is copied", read(dir / "ngosen.conf") == "Mode=Sen\n");
        check("lotus.conf stays for older versions", std::filesystem::exists(dir / "lotus.conf"));
        check("an existing new file is not overwritten", read(dir / "ngosen-macro-table.conf") == "new macros\n");
        check("a missing old file creates nothing", !std::filesystem::exists(dir / "ngosen-app-rules.conf"));
        check("the dictionary is copied into a new folder", read(dir / "ngosen/vietnamese.cm.dict") == "từ\n");
        check("a second pass copies nothing", ngosen::copyLegacyFiles(dir, {{"lotus.conf", "ngosen.conf"}}).empty());
        std::filesystem::remove_all(dir);
    }

} // namespace

int main() {
    testRename();
    testBothListed();
    testOtherGroups();
    testCopyFiles();
    if (failures == 0) {
        std::cout << "ngosen-migration: ok\n";
    }
    return failures == 0 ? 0 : 1;
}
