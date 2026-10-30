// SPDX-License-Identifier: GPL-3.0-or-later
//
// The toggle-key switches saved for each app are read back at start, and the settings window's
// Defaults forget them.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

    int         failures = 0;

    std::string modeName(fcitx::NgoSenMode mode) {
        switch (mode) {
            case fcitx::NgoSenMode::Off: return "Off";
            case fcitx::NgoSenMode::Preedit: return "Preedit";
            case fcitx::NgoSenMode::Sen: return "Sen";
            case fcitx::NgoSenMode::Emoji: return "Emoji";
            default: return "Unknown";
        }
    }

    void expectMode(const std::string& step, fcitx::NgoSenMode expected) {
        const fcitx::NgoSenMode actual = ::realMode.load();
        if (actual != expected) {
            std::cerr << "FAIL: " << step << ": expected " << modeName(expected) << ", got " << modeName(actual) << '\n';
            ++failures;
        }
    }

    void focus(fcitx::NgoSenEngine& engine, TestInputContext& context) {
        context.focusIn();
        fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, event);
    }

} // namespace

int main() {
    const char* testName = "fcitx5-ngosen-app-switch-memory";
    configureTestPaths(testName);
    // The engine reads both files when it starts.
    const auto confDir = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf";
    std::filesystem::create_directories(confDir);
    std::ofstream(confDir / "ngosen.conf", std::ios::trunc) << "Mode=Sen\nVietnameseOn=True\nInputMethod=Telex\n";
    std::ofstream(confDir / "ngosen-app-switch.conf", std::ios::trunc) << "[Switches]\n[Switches/0]\nApp=kate\nVietnamese=False\n";

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);

    TestInputContext    kate(&testInstance.instance, "kate");
    TestInputContext    gedit(&testInstance.instance, "gedit");
    focus(engine, kate);
    expectMode("kate starts in the English it was switched to", fcitx::NgoSenMode::Off);
    focus(engine, gedit);
    expectMode("gedit follows the last switch", fcitx::NgoSenMode::Sen);

    focus(engine, kate);
    fcitx::RawConfig noSwitches;
    noSwitches.get("Switches", true);
    engine.setSubConfig("app_switches", noSwitches);
    expectMode("Defaults turns the focused kate to the last switch at once", fcitx::NgoSenMode::Sen);

    return failures == 0 ? 0 : 1;
}
