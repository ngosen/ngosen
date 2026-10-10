// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mode-name-migration.cpp
 * @brief Headless test: every former name of the direct typing mode (Uinput, and before that Smooth,
 *        Slow, Super Smooth, Minecraft) and the removed Surrounding Text mode load as Sen, with the
 *        menu visibility and shortcut of Uinput kept.
 *
 * The mode is persisted by name in ngosen.conf and by number in ngosen-app-rules.conf. An unknown
 * name makes fcitx keep the option default (Preedit), so without a migration the user would
 * silently switch to preedit typing after the upgrade.
 */

#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

} // namespace

int main() {
    const char* testName = "fcitx5-ngosen-mode-name-migration";
    configureTestPaths(testName);

    // ngosen.conf on disk is read by the engine constructor: the upgrade path of an installed user.
    const auto confFile = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf/ngosen.conf";

    // A fresh install has no ngosen.conf yet.
    std::filesystem::remove(confFile);
    {
        TestInstance        freshInstance;
        fcitx::NgoSenEngine freshEngine(&freshInstance.instance);
        check("a fresh install starts in Sen", freshEngine.config().mode.value() == fcitx::NgoSenMode::Sen);
    }
    {
        std::ofstream file(confFile, std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "cannot write " << confFile << '\n';
            return 1;
        }
        file << "Mode=\"Uinput (Super Smooth)\"\n";
        file << "ModeOrder=Smooth,Uinput,Minecraft,SurroundingText,Preedit,Emoji,Off,SuperSmooth,Default\n";
        file << "ShowModeUinput=False\n";
        file << "ShortcutUinput=z\n";
    }

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    check("ngosen.conf with \"Uinput (Super Smooth)\" loads as Sen", engine.config().mode.value() == fcitx::NgoSenMode::Sen);
    check("old ModeOrder lists Sen once", *engine.config().modeOrder == "Sen,Preedit,Emoji,Off,Default");
    check("ShowModeUinput=False hides Sen", !*engine.config().showModeSen);
    check("ShortcutUinput=z becomes the Sen shortcut", *engine.config().shortcutSen == "z");

    {
        fcitx::RawConfig config;
        config.setValueByPath("ModeOrder", "Preedit,SuperSmooth,Off,Uinput,Smooth,SurroundingText");
        engine.setConfig(config);
        check("setConfig ModeOrder merges former names into Sen", *engine.config().modeOrder == "Preedit,Sen,Off");
    }

    // setConfig is the path the settings GUI and fcitx5-configtool use.
    for (const char* legacy : {"Uinput", "Uinput (Smooth)", "Uinput (Slow)", "Uinput (Super Smooth)", "Minecraft", "Surrounding Text"}) {
        fcitx::RawConfig config;
        config.setValueByPath("Mode", legacy);
        engine.setConfig(config);
        check(std::string("setConfig Mode=\"") + legacy + "\" gives Sen", engine.config().mode.value() == fcitx::NgoSenMode::Sen);
    }

    // Per-app rules store the mode as a number: 1 = Smooth, 2 = Uinput, 3 = Super Smooth, 4 = Surrounding
    // Text, 8 = Minecraft.
    for (int legacy : {1, 2, 3, 4, 8}) {
        check("app rule mode " + std::to_string(legacy) + " gives Sen", fcitx::intToMode(legacy) == fcitx::NgoSenMode::Sen);
    }

    return failures == 0 ? 0 : 1;
}
