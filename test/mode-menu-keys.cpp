// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mode menu and the cycle key, driven by key presses: open the menu, move in it, pick a mode by
// its shortcut, close it with Escape, type the hotkey's own character, and step to the next mode.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <fcitx/candidatelist.h>

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

    void press(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym sym) {
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
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("CycleModeKey/0", "F12");
    engine.setConfig(config);

    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "gedit");
    context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    // Sen, Preedit, Emoji, Off, Default typing, and "type `".
    press(engine, entry, *context, FcitxKey_grave);
    auto list = menu(*context);
    check(list && list->totalSize() == 6, "menu opens with six items", list ? std::to_string(list->totalSize()) : "no menu");
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

    return failures == 0 ? 0 : 1;
}
