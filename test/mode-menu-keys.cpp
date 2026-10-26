// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mode menu and the cycle key, driven by key presses: open the menu, move in it, pick a mode by
// its shortcut, close it with Escape, type the hotkey's own character, and step to the next mode.
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

    int  failures = 0;

    void check(bool ok, const std::string& what, const std::string& actual) {
        if (!ok) {
            std::cerr << "FAIL: " << what << "\n  got: " << actual << '\n';
            ++failures;
        }
    }

    void press(fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym sym) {
        fcitx::KeyEvent down(&context, fcitx::Key(sym), false);
        engine.keyEvent(entry, down);
        fcitx::KeyEvent up(&context, fcitx::Key(sym), true);
        engine.keyEvent(entry, up);
    }

    // The menu, as opposed to the one-line notice shown after a mode change.
    std::shared_ptr<fcitx::CommonCandidateList> menu(TestInputContext& context) {
        auto list = std::dynamic_pointer_cast<fcitx::CommonCandidateList>(context.inputPanel().candidateList());
        return list && list->totalSize() > 1 ? list : nullptr;
    }

    std::string label(const fcitx::CommonCandidateList& list, int index) {
        return list.candidateFromAll(index).text().toString();
    }

    std::string modeName(ngosen::Mode mode) {
        switch (mode) {
            case ngosen::Mode::Off: return "Off";
            case ngosen::Mode::Sen: return "Sen";
            case ngosen::Mode::Preedit: return "Preedit";
            case ngosen::Mode::Emoji: return "Emoji";
        }
        return "?";
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-mode-menu-keys");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("CycleModeKey/0", "F12");
    engine.setConfig(config);

    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "gedit");
    context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    // Sen, Preedit, Emoji, Off, Default typing, the Vim mode switch, and "type `".
    press(engine, entry, *context, FcitxKey_grave);
    auto list = menu(*context);
    check(list && list->totalSize() == 7, "menu opens with seven items", list ? std::to_string(list->totalSize()) : "no menu");
    check(list && label(*list, 5) == "   [v] Vim mode: off", "the Vim mode switch starts off", list ? label(*list, 5) : "no menu");
    // gedit has no rule of its own, so it follows the default.
    check(list && list->globalCursorIndex() == 4, "Default typing is highlighted", list ? std::to_string(list->globalCursorIndex()) : "no menu");
    press(engine, entry, *context, FcitxKey_Down);
    check(list && list->globalCursorIndex() == 5, "Down moves the highlight", list ? std::to_string(list->globalCursorIndex()) : "no menu");

    press(engine, entry, *context, FcitxKey_1);
    check(!menu(*context), "picking a mode closes the menu", "menu still open");
    check(realMode.load() == ngosen::Mode::Sen, "shortcut 1 picks Sen", modeName(realMode.load()));

    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_Escape);
    check(!menu(*context), "Escape closes the menu", "menu still open");
    check(realMode.load() == ngosen::Mode::Sen, "Escape keeps the mode", modeName(realMode.load()));

    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_grave);
    check(!menu(*context), "typing the hotkey closes the menu", "menu still open");
    check(context->commits() == std::vector<std::string>{"`"}, "the hotkey a second time types it", std::to_string(context->commits().size()) + " commits");

    press(engine, entry, *context, FcitxKey_F12);
    check(realMode.load() == ngosen::Mode::Preedit, "the cycle key steps from Sen to Preedit", modeName(realMode.load()));

    // v switches Vim mode for the app and keeps the typing mode; the choice is saved for gedit.
    const auto vimApps = [] {
        std::ifstream file(std::filesystem::temp_directory_path() / "fcitx5-ngosen-mode-menu-keys/config/fcitx5/conf/ngosen-vim-apps.conf");
        std::string   line;
        std::string   apps;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#')
                apps += line + ";";
        }
        return apps;
    };
    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_v);
    check(!menu(*context), "v closes the menu", "menu still open");
    check(realMode.load() == ngosen::Mode::Preedit, "v keeps the typing mode", modeName(realMode.load()));
    check(vimApps() == "gedit;", "v saves Vim mode for gedit", vimApps());
    check(context->commits().size() == 1, "v types nothing", std::to_string(context->commits().size()) + " commits");
    press(engine, entry, *context, FcitxKey_grave);
    list = menu(*context);
    check(list && label(*list, 5) == "   [v] Vim mode: on", "the menu shows Vim mode on", list ? label(*list, 5) : "no menu");
    press(engine, entry, *context, FcitxKey_v);
    check(vimApps().empty(), "v again turns Vim mode off", vimApps());

    // With a label, GNOME's kimpanel shows text in the top bar instead of the Ngó Sen icon.
    for (const auto mode : {ngosen::Mode::Sen, ngosen::Mode::Off, ngosen::Mode::Emoji}) {
        realMode         = mode;
        const auto label = engine.subModeLabel(entry, *context);
        check(label.empty(), "no panel label in " + modeName(mode), label);
        // The switch popup still has to tell Off and the emoji picker apart.
        const auto subMode  = engine.subMode(entry, *context);
        const auto expected = mode == ngosen::Mode::Sen ? "Telex" : mode == ngosen::Mode::Off ? "OFF" : "Emoji Picker";
        check(subMode == expected, "switch popup names " + modeName(mode), subMode);
    }

    return failures == 0 ? 0 : 1;
}
