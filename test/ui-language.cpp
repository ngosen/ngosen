// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tray menu and the hints next to the cursor speak the language picked in Ngó Sen's settings,
// Vietnamese unless changed.
#include "ngosen-engine.h"
#include "ngosen-fcitx-host.h"
#include "ngosen-ui-text.h"
#include "test-input-context.h"

#include <fcitx/action.h>
#include <fcitx/statusarea.h>

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

    std::string label(TestInputContext& context, const std::string& name) {
        for (auto* action : context.statusArea().actions(fcitx::StatusGroup::InputMethod))
            if (action->name() == name)
                return action->shortText(&context);
        return "";
    }

    fcitx::Action* action(TestInputContext& context, const std::string& name) {
        for (auto* action : context.statusArea().actions(fcitx::StatusGroup::InputMethod))
            if (action->name() == name)
                return action;
        return nullptr;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-ui-language");
    check("the Vietnamese catalog loads", ngosen::loadUiCatalog(NGOSEN_TEST_CATALOG));
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);

    TestInputContext    context(&testInstance.instance);
    context.focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent in(&context, fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, in);

    check("the menu is Vietnamese by default", label(context, "ngosen-settings") == "Cài đặt" && label(context, "ngosen-charset") == "Bảng mã");
    const auto hint = [&] { return ngosen::FcitxHost(&context, &testInstance.instance).translate("Tab or Esc: put back what was typed"); };
    check("the Cứu chữ hint is Vietnamese by default", hint() == "Cứu chữ: Tab hoặc Esc để trả lại chữ đã gõ");

    fcitx::RawConfig english;
    english.setValueByPath("UiLanguage", "English");
    engine.setConfig(english);
    check("picking English changes the menu at once", label(context, "ngosen-settings") == "Settings" && label(context, "ngosen-input-method") == "Input Method");
    check("picking English changes the hint", hint() == "Tab or Esc: put back what was typed");

    const bool spellCheck = label(context, "ngosen-spellcheck").find("✔") == 0;
    action(context, "ngosen-spellcheck")->activate(&context);
    check("a toggled entry keeps the picked language", label(context, "ngosen-spellcheck") == std::string(spellCheck ? "✖ " : "✔ ") + "Spell Check");

    fcitx::RawConfig vietnamese;
    vietnamese.setValueByPath("UiLanguage", "Vietnamese");
    engine.setConfig(vietnamese);
    check("picking Vietnamese changes it back",
          label(context, "ngosen-charset") == "Bảng mã" && label(context, "ngosen-spellcheck").find("Kiểm tra chính tả") != std::string::npos);

    return failures == 0 ? 0 : 1;
}
