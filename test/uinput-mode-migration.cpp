// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uinput-mode-migration.cpp
 * @brief Headless test: the three former uinput modes (Smooth, Slow, Super Smooth) load as the
 *        single Uinput mode.
 *
 * The mode is persisted by name in lotus.conf and by number in lotus-app-rules.conf. An unknown
 * name makes fcitx keep the option default (Preedit), so without a migration a user on a former
 * uinput mode would silently switch to preedit typing after the upgrade.
 */

#include "lotus-engine.h"
#include "lotus-utils.h"
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
    const char* testName = "fcitx5-lotus-uinput-mode-migration";
    configureTestPaths(testName);

    // lotus.conf on disk is read by the engine constructor: the upgrade path of an installed user.
    const auto confFile = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf/lotus.conf";
    {
        std::ofstream file(confFile, std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "cannot write " << confFile << '\n';
            return 1;
        }
        file << "Mode=\"Uinput (Super Smooth)\"\n";
        file << "ModeOrder=Smooth,Uinput,Minecraft,SurroundingText,Preedit,Emoji,Off,SuperSmooth,Default\n";
    }

    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    check("lotus.conf with \"Uinput (Super Smooth)\" loads as Uinput", engine.config().mode.value() == fcitx::LotusMode::Uinput);
    check("pre-merge ModeOrder lists Uinput once", *engine.config().modeOrder == "Uinput,SurroundingText,Preedit,Emoji,Off,Default");

    {
        fcitx::RawConfig config;
        config.setValueByPath("ModeOrder", "Preedit,SuperSmooth,Off,Smooth");
        engine.setConfig(config);
        check("setConfig ModeOrder merges former uinput modes", *engine.config().modeOrder == "Preedit,Uinput,Off");
    }

    // setConfig is the path the settings GUI and fcitx5-configtool use.
    for (const char* legacy : {"Uinput (Smooth)", "Uinput (Slow)", "Uinput (Super Smooth)", "Minecraft"}) {
        fcitx::RawConfig config;
        config.setValueByPath("Mode", legacy);
        engine.setConfig(config);
        check(std::string("setConfig Mode=\"") + legacy + "\" gives Uinput", engine.config().mode.value() == fcitx::LotusMode::Uinput);
    }

    // Per-app rules store the mode as a number: 1 = Smooth, 2 = Slow, 3 = Super Smooth, 8 = Minecraft.
    for (int legacy : {1, 2, 3, 8}) {
        check("app rule mode " + std::to_string(legacy) + " gives Uinput", fcitx::intToMode(legacy) == fcitx::LotusMode::Uinput);
    }

    return failures == 0 ? 0 : 1;
}
