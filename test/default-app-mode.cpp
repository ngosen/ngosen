// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file default-app-mode.cpp
 * @brief Headless test: deepin-editor types in Preedit unless the user picked a mode for it.
 *
 * deepin-editor stalls between edits, and fcitx5-qt applies our commit before the keys let through
 * ahead of it, so fast typing in Sen scrambles words there. Preedit sends no keys and types it right.
 * argv[1] = "override" writes a user rule picking Sen for deepin-editor first.
 */

#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <fcitx/candidatelist.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

    void setGlobal(fcitx::NgoSenEngine& engine, bool vietnameseOn) {
        fcitx::RawConfig config;
        config.setValueByPath("Mode", "Sen");
        config.setValueByPath("VietnameseOn", vietnameseOn ? "True" : "False");
        config.setValueByPath("InputMethod", "Telex");
        engine.setConfig(config);
    }

    void focus(fcitx::NgoSenEngine& engine, TestInputContext& context) {
        context.focusIn();
        fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, event);
    }

    void press(fcitx::NgoSenEngine& engine, TestInputContext& context, fcitx::KeySym sym) {
        fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::KeyEvent         down(&context, fcitx::Key(sym), false);
        engine.keyEvent(entry, down);
        fcitx::KeyEvent up(&context, fcitx::Key(sym), true);
        engine.keyEvent(entry, up);
    }

} // namespace

int main(int argc, char** argv) {
    const bool  override = argc > 1 && std::string(argv[1]) == "override";
    const char* testName = override ? "fcitx5-ngosen-default-app-mode-override" : "fcitx5-ngosen-default-app-mode";
    configureTestPaths(testName);

    if (override) {
        // loadAppRules runs in the engine constructor, so the rule must be on disk first.
        const auto    rulesFile = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf/ngosen-app-rules.conf";
        std::ofstream file(rulesFile, std::ios::trunc);
        file << "deepin-editor=2\n";
    }

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    setGlobal(engine, true);

    TestInputContext other(&testInstance.instance, "test");
    focus(engine, other);
    expectMode("another app keeps the global Sen", fcitx::NgoSenMode::Sen);

    TestInputContext editor(&testInstance.instance, "deepin-editor");
    focus(engine, editor);
    if (override) {
        expectMode("the user's Sen rule for deepin-editor wins", fcitx::NgoSenMode::Sen);
        return failures == 0 ? 0 : 1;
    }
    expectMode("deepin-editor types in Preedit", fcitx::NgoSenMode::Preedit);

    // With no rule of its own the app follows its default, so the menu marks "Default typing"
    // (Sen, Preedit, Emoji, Off, Default typing, "type `"), and picking it keeps Preedit.
    press(engine, editor, FcitxKey_grave);
    auto list = std::dynamic_pointer_cast<fcitx::CommonCandidateList>(editor.inputPanel().candidateList());
    if (!list || list->globalCursorIndex() != 4) {
        std::cerr << "FAIL: the menu marks Default typing: cursor " << (list ? std::to_string(list->globalCursorIndex()) : "no menu") << '\n';
        ++failures;
    }
    press(engine, editor, FcitxKey_r);
    expectMode("Default typing keeps Preedit in deepin-editor", fcitx::NgoSenMode::Preedit);

    TestInputContext desktopNamed(&testInstance.instance, "deepin-editor.desktop");
    focus(engine, desktopNamed);
    expectMode("deepin-editor.desktop types in Preedit", fcitx::NgoSenMode::Preedit);

    setGlobal(engine, false);
    focus(engine, editor);
    expectMode("English stays English in deepin-editor", fcitx::NgoSenMode::Off);

    return failures == 0 ? 0 : 1;
}
