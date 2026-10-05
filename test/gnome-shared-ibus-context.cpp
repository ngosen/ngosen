// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file gnome-shared-ibus-context.cpp
 * @brief Headless regression test for GNOME's IBus path.
 *
 * GNOME Shell multiplexes every window through one IBus input context and only
 * relabels its program name ("kitty.desktop", "firefox.desktop") on focus. Two
 * consequences:
 *
 * 1. A client without surrounding support (kitty) never sends surrounding text,
 *    so fcitx5 keeps the previous window's snapshot as valid. Lotus read it as
 *    the terminal's text and took the wrong replacement path. The engine must
 *    drop surrounding text the client does not advertise.
 * 2. Program names carry a ".desktop" suffix, so per-app rules written as
 *    "firefox" never matched. The bare name must be used as a fallback, while
 *    an exact "foo.desktop" rule still wins.
 */

#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual, const std::string& meaning) {
        std::cerr << "Step: " << step << '\n';
        std::cerr << "Expected: " << expected << '\n';
        std::cerr << "Actual: " << actual << '\n';
        std::cerr << "Meaning: " << meaning << '\n';
    }

    std::string modeName(fcitx::LotusMode mode) {
        switch (mode) {
            case fcitx::LotusMode::Off: return "Off";
            case fcitx::LotusMode::Preedit: return "Preedit";
            case fcitx::LotusMode::Uinput: return "Uinput";
            case fcitx::LotusMode::SurroundingText: return "SurroundingText";
            case fcitx::LotusMode::Emoji: return "Emoji";
            default: return "Unknown";
        }
    }

    bool checkStrip(const std::string& in, const std::string& expected) {
        const auto actual = stripDesktopSuffix(in);
        if (actual != expected) {
            reportFailure("stripDesktopSuffix(\"" + in + "\")", expected, actual, "GNOME program names must map to the bare name rules use");
            return false;
        }
        return true;
    }

} // namespace

int main() {
    const char* testName = "fcitx5-lotus-gnome-shared-ibus-context";
    configureTestPaths(testName);

    if (!checkStrip("firefox.desktop", "firefox") || !checkStrip("org.gnome.TextEditor.desktop", "org.gnome.TextEditor") || !checkStrip("firefox", "firefox") ||
        !checkStrip(".desktop", ".desktop")) {
        return 1;
    }

    const auto rulesFile = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf/lotus-app-rules.conf";
    {
        std::ofstream file(rulesFile, std::ios::trunc);
        if (!file.is_open()) {
            reportFailure("write app rules file", "file open", rulesFile.string(), "the test needs the app rules on disk");
            return 1;
        }
        file << "firefox=0\nkitty=0\nkitty.desktop=5\n";
    }

    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);

    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");

    // Rule lookup: "firefox.desktop" falls back to the "firefox" rule.
    {
        auto context = std::make_unique<TestInputContext>(&testInstance.instance, "firefox.desktop");
        context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
        context->focusIn();
        fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, focus);
        if (::realMode.load() != fcitx::LotusMode::Off) {
            reportFailure("rule for firefox.desktop", "realMode=Off", "realMode=" + modeName(::realMode.load()), "a \"firefox\" rule must apply to GNOME's \"firefox.desktop\"");
            return 1;
        }
        // The client advertises surrounding text, so its snapshot must be kept.
        context->surroundingText().setText("abc", 3, 3);
        fcitx::KeyEvent key(context.get(), fcitx::Key(FcitxKey_a), false);
        engine.keyEvent(entry, key);
        if (!context->surroundingText().isValid()) {
            reportFailure("keep advertised surrounding", "valid", "invalid", "only surrounding text the client cannot have sent may be dropped");
            return 1;
        }
        context->focusOut();
    }

    // Exact "kitty.desktop" rule wins over the bare "kitty" rule.
    {
        auto context = std::make_unique<TestInputContext>(&testInstance.instance, "kitty.desktop");
        context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
        context->focusIn();
        fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, focus);
        if (::realMode.load() != fcitx::LotusMode::Preedit) {
            reportFailure("exact rule for kitty.desktop", "realMode=Preedit", "realMode=" + modeName(::realMode.load()),
                          "an exact \".desktop\" rule must override the bare-name rule");
            return 1;
        }
        context->focusOut();
    }

    // Stale snapshot: a client without surrounding support inherits text left by the previous window.
    {
        auto context = std::make_unique<TestInputContext>(&testInstance.instance, "ngosen-do.desktop");
        context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
        context->surroundingText().setText("abc\n", 4, 4);
        context->focusIn();
        fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, focus);
        if (context->surroundingText().isValid()) {
            reportFailure("drop stale surrounding on focus", "invalid", "valid text=[" + context->surroundingText().text() + "]",
                          "a client without surrounding support cannot own a surrounding snapshot");
            return 1;
        }
        // The same stale text may be pushed again between keys; the key path must drop it too.
        context->surroundingText().setText("\n", 1, 1);
        fcitx::KeyEvent key(context.get(), fcitx::Key(FcitxKey_a), false);
        engine.keyEvent(entry, key);
        if (context->surroundingText().isValid()) {
            reportFailure("drop stale surrounding on key", "invalid", "valid", "the key path must not read another window's text");
            return 1;
        }
        context->focusOut();
    }

    return 0;
}
