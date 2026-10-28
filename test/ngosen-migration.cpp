// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ngosen-migration.cpp
 * @brief Unit test: settings saved under the lotus names carry over to the ngosen names, and a
 * keyboard left next to Ngó Sen is dropped from the profile.
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

    void testDropKeyboard() {
        auto profile = parse("[Groups/0]\nName=Default\nDefault Layout=\nDefaultIM=keyboard-de-neo\n\n"
                             "[Groups/0/Items/0]\nName=keyboard-de-neo\nLayout=\n\n"
                             "[Groups/0/Items/1]\nName=ngosen\nLayout=\n\n"
                             "[GroupOrder]\n0=Default\n");
        check("a keyboard next to Ngó Sen is dropped", ngosen::dropLoneKeyboard(profile, "ngosen"));
        check("Ngó Sen is the only item", value(profile, "Groups/0/Items/0/Name") == "ngosen" && !profile.get("Groups/0/Items/1"));
        check("Ngó Sen is the default input method", value(profile, "Groups/0/DefaultIM") == "ngosen");
        check("the keyboard's layout becomes the group's", value(profile, "Groups/0/Default Layout") == "de-neo");
        check("a second pass changes nothing", !ngosen::dropLoneKeyboard(profile, "ngosen"));

        profile = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n"
                        "[Groups/0/Items/0]\nName=ngosen\nLayout=\n\n"
                        "[Groups/0/Items/1]\nName=keyboard-us\nLayout=\n");
        check("a keyboard after Ngó Sen is dropped", ngosen::dropLoneKeyboard(profile, "ngosen"));
        check("the group layout is kept", value(profile, "Groups/0/Default Layout") == "us");

        profile = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n"
                        "[Groups/0/Items/0]\nName=keyboard-de\nLayout=\n\n"
                        "[Groups/0/Items/1]\nName=ngosen\nLayout=\n");
        check("a keyboard on another layout than the group's stays", !ngosen::dropLoneKeyboard(profile, "ngosen"));
    }

    void testListsInputMethod() {
        auto profile = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n"
                             "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
                             "[Groups/1]\nName=Other\nDefault Layout=us\nDefaultIM=ngosen\n\n"
                             "[Groups/1/Items/0]\nName=ngosen\nLayout=\n");
        check("Ngó Sen in a second group is found", ngosen::listsInputMethod(profile, "ngosen"));
        check("an input method not listed is not found", !ngosen::listsInputMethod(profile, "lotus"));
        fcitx::RawConfig empty;
        check("no profile lists nothing", !ngosen::listsInputMethod(empty, "ngosen"));
    }

    void testCopySwitchKeys() {
        auto             fcitxConfig = parse("[Hotkey/TriggerKeys]\n0=Super+space\n1=Zenkaku_Hankaku\n");
        fcitx::RawConfig settings;
        check("fcitx5's switch keys become the toggle key", ngosen::copySwitchKeys(fcitxConfig, settings));
        check("the first key is copied", value(settings, "ToggleKey/0") == "Super+space");
        check("the second key is copied", value(settings, "ToggleKey/1") == "Zenkaku_Hankaku");

        auto chosen = parse("[ToggleKey]\n0=Alt+z\n");
        check("a toggle key already set stays", !ngosen::copySwitchKeys(fcitxConfig, chosen) && value(chosen, "ToggleKey/0") == "Alt+z");
        fcitx::RawConfig defaults;
        check("no switch key in fcitx5's config copies nothing", !ngosen::copySwitchKeys(defaults, settings));
    }

    void testUpgradeTurnsRetypeOn() {
        // fcitx5 saves every option, so a file saved before the upgrade holds the hidden default.
        auto saved = parse("Mode=Sen\nRetypeAfterSwitch=False\n");
        check("settings from before SettingsVersion are upgraded", ngosen::upgradeSettings(saved, true));
        check("retyping is turned on for an upgraded user", value(saved, "RetypeAfterSwitch") == "True");

        auto chosen = parse("SettingsVersion=1\nRetypeAfterSwitch=False\n");
        check("current settings are left alone", !ngosen::upgradeSettings(chosen, true));
        check("retyping turned off in the settings window stays off", value(chosen, "RetypeAfterSwitch") == "False");

        fcitx::RawConfig fresh;
        ngosen::upgradeSettings(fresh, false);
        check("a fresh install keeps the built-in default", value(fresh, "RetypeAfterSwitch") == "<missing>");
    }

    void testReleaseAnnouncedOnce() {
        auto upgraded = parse("SettingsVersion=1\n");
        check("an upgrade to a feature release is announced", ngosen::releaseToAnnounce(upgraded, true, "1.7.0") == "1.7");
        check("the announced release is remembered", value(upgraded, "AnnouncedVersion") == "1.7");
        check("a bug-fix release is not announced", ngosen::releaseToAnnounce(upgraded, true, "1.7.2").empty());
        check("the next feature release is announced again", ngosen::releaseToAnnounce(upgraded, true, "1.8.0") == "1.8");
        check("going back to an older version stays quiet", ngosen::releaseToAnnounce(upgraded, true, "1.7.3").empty());

        fcitx::RawConfig fresh;
        check("a fresh install is not announced", ngosen::releaseToAnnounce(fresh, false, "1.7.0").empty());
        check("a fresh install remembers its release", value(fresh, "AnnouncedVersion") == "1.7");
        check("the first bug-fix release after a fresh install is quiet", ngosen::releaseToAnnounce(fresh, true, "1.7.1").empty());
    }

    void testKeepOtherInputMethods() {
        auto twoLayouts = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n"
                                "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
                                "[Groups/0/Items/1]\nName=keyboard-fr\nLayout=\n\n"
                                "[Groups/0/Items/2]\nName=ngosen\nLayout=\n");
        check("two keyboards are kept", !ngosen::dropLoneKeyboard(twoLayouts, "ngosen"));
        auto mozc = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=ngosen\n\n"
                          "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
                          "[Groups/0/Items/1]\nName=mozc\nLayout=\n\n"
                          "[Groups/0/Items/2]\nName=ngosen\nLayout=\n");
        check("a group with another input method is kept", !ngosen::dropLoneKeyboard(mozc, "ngosen"));
        auto mozcOnly = parse("[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=mozc\n\n"
                              "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
                              "[Groups/0/Items/1]\nName=mozc\nLayout=\n");
        check("a group without Ngó Sen is kept", !ngosen::dropLoneKeyboard(mozcOnly, "ngosen"));
        check("its keyboard stays", value(mozcOnly, "Groups/0/Items/0/Name") == "keyboard-us");
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

    // A /proc lookalike: numbered folders with a comm file, owned by the user running the test.
    void testProcessesNamed() {
        const auto dir = std::filesystem::temp_directory_path() / "ngosen-migration-proc";
        std::filesystem::remove_all(dir);
        for (const auto& [pid, comm] : {std::pair{"100", "fcitx5"}, {"101", "bash"}, {"102", "fcitx5"}, {"103", "fcitx5-remote"}}) {
            std::filesystem::create_directories(dir / pid);
            std::ofstream(dir / pid / "comm") << comm << '\n';
        }
        std::filesystem::create_directories(dir / "self");
        std::ofstream(dir / "self" / "comm") << "fcitx5\n";
        std::filesystem::create_directories(dir / "104");

        auto pids = ngosen::processesNamed(dir, "fcitx5", getuid(), 102);
        check("one other fcitx5 is found", pids.size() == 1 && pids.front() == 100);
        pids = ngosen::processesNamed(dir, "fcitx5", getuid(), 1);
        check("both fcitx5 processes are found", pids.size() == 2);
        check("another user's processes are skipped", ngosen::processesNamed(dir, "fcitx5", getuid() + 1, 1).empty());
        check("a missing folder finds nothing", ngosen::processesNamed(dir / "missing", "fcitx5", getuid(), 1).empty());
        std::filesystem::remove_all(dir);
    }

} // namespace

int main() {
    testRename();
    testBothListed();
    testOtherGroups();
    testDropKeyboard();
    testKeepOtherInputMethods();
    testListsInputMethod();
    testCopySwitchKeys();
    testUpgradeTurnsRetypeOn();
    testReleaseAnnouncedOnce();
    testCopyFiles();
    testProcessesNamed();
    if (failures == 0) {
        std::cout << "ngosen-migration: ok\n";
    }
    return failures == 0 ? 0 : 1;
}
