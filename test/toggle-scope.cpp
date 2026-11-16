// SPDX-License-Identifier: GPL-3.0-or-later
//
// With the toggle key set to switch all apps, one press switches every app, and an app the menu set to
// OFF stays in English: the key opens the menu there instead of switching the others.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <fcitx/candidatelist.h>

#include <filesystem>
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

    std::string modeName(ngosen::Mode mode) {
        switch (mode) {
            case ngosen::Mode::Off: return "Off";
            case ngosen::Mode::Sen: return "Sen";
            case ngosen::Mode::Preedit: return "Preedit";
        }
        return "?";
    }

    struct Keys {
        fcitx::NgoSenEngine&           engine;
        const fcitx::InputMethodEntry& entry;

        void                           send(TestInputContext& context, fcitx::Key key, bool release) {
            fcitx::KeyEvent event(&context, key, release);
            engine.keyEvent(entry, event);
        }

        void tap(TestInputContext& context, const char* key) {
            send(context, fcitx::Key(key), false);
            send(context, fcitx::Key(key), true);
        }

        void ctrlShift(TestInputContext& context) {
            const fcitx::KeyStates ctrl{fcitx::KeyState::Ctrl};
            const fcitx::KeyStates both{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift};
            send(context, fcitx::Key(FcitxKey_Control_L), false);
            send(context, fcitx::Key(FcitxKey_Shift_L, ctrl), false);
            send(context, fcitx::Key(FcitxKey_Shift_L, both), true);
            send(context, fcitx::Key(FcitxKey_Control_L, ctrl), true);
        }

        void focus(TestInputContext& context) {
            context.focusIn();
            fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, event);
        }
    };

    bool menuOpen(TestInputContext& context) {
        auto list = context.inputPanel().candidateList();
        return list && list->size() > 1;
    }

    bool vietnameseOn(const fcitx::NgoSenEngine& engine) {
        fcitx::RawConfig raw;
        engine.getConfig()->save(raw);
        const auto* value = raw.valueByPath("VietnameseOn");
        return value != nullptr && *value == "True";
    }

    std::string configFile(const char* name) {
        return std::string(getenv("XDG_CONFIG_HOME")) + "/fcitx5/conf/" + name;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-toggle-scope");
    TestInstance            testInstance;
    fcitx::NgoSenEngine     engine(&testInstance.instance);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    Keys                    keys{engine, entry};

    auto                    gedit = std::make_unique<TestInputContext>(&testInstance.instance, "gedit");
    auto                    kate  = std::make_unique<TestInputContext>(&testInstance.instance, "kate");
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "a new install starts in Vietnamese", modeName(realMode.load()));

    keys.ctrlShift(*gedit);
    check(realMode.load() == ngosen::Mode::Off, "Ctrl+Shift switches to English", modeName(realMode.load()));
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Off, "another app follows the switch", modeName(realMode.load()));
    keys.ctrlShift(*kate);
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "the app switched first follows the next switch too", modeName(realMode.load()));
    check(!std::filesystem::exists(configFile("ngosen-app-switch.conf")), "no app keeps a switch of its own", "switch file saved");

    // OFF picked from the menu keeps gedit in English while the others stay in Vietnamese.
    keys.tap(*gedit, "grave");
    keys.tap(*gedit, "e");
    check(realMode.load() == ngosen::Mode::Off, "the menu sets OFF for gedit", modeName(realMode.load()));
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Sen, "kate stays in Vietnamese", modeName(realMode.load()));

    keys.focus(*gedit);
    keys.ctrlShift(*gedit);
    check(realMode.load() == ngosen::Mode::Off, "Ctrl+Shift does not switch an app the menu keeps in English", modeName(realMode.load()));
    check(menuOpen(*gedit), "Ctrl+Shift there opens the menu", "no menu");
    check(vietnameseOn(engine), "Ctrl+Shift there leaves the other apps in Vietnamese", "switched off");
    keys.tap(*gedit, "Escape");
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Sen, "kate is still in Vietnamese", modeName(realMode.load()));

    // The switch, the rule and the scope are separate: each app keeping its switch brings OFF back as before.
    fcitx::RawConfig eachApp;
    eachApp.setValueByPath("ToggleScope", "Each app");
    engine.setConfig(eachApp);
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Off, "gedit's OFF stays with each app keeping its switch", modeName(realMode.load()));
    fcitx::RawConfig allApps;
    allApps.setValueByPath("ToggleScope", "All apps");
    engine.setConfig(allApps);

    // Sen picked from that menu types Vietnamese in gedit.
    keys.focus(*gedit);
    keys.ctrlShift(*gedit);
    keys.tap(*gedit, "1");
    check(realMode.load() == ngosen::Mode::Sen, "Sen from the menu types Vietnamese in gedit", modeName(realMode.load()));

    // Preedit picked for kate is how it types Vietnamese; the toggle key still switches it.
    keys.focus(*kate);
    keys.tap(*kate, "grave");
    keys.tap(*kate, "q");
    check(realMode.load() == ngosen::Mode::Preedit, "the menu sets Preedit for kate", modeName(realMode.load()));
    keys.ctrlShift(*kate);
    check(realMode.load() == ngosen::Mode::Off, "Ctrl+Shift switches kate's Preedit to English", modeName(realMode.load()));
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Off, "gedit follows kate to English", modeName(realMode.load()));
    keys.ctrlShift(*gedit);
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Preedit, "kate comes back in Preedit", modeName(realMode.load()));

    // A Vietnamese mode picked while all apps are in English turns Vietnamese on; OFF only stops that app.
    fcitx::RawConfig cycle;
    cycle.setValueByPath("CycleModeKey/0", "F12");
    engine.setConfig(cycle);
    auto notes = std::make_unique<TestInputContext>(&testInstance.instance, "notes");
    auto mail  = std::make_unique<TestInputContext>(&testInstance.instance, "mail");
    keys.focus(*notes);
    keys.ctrlShift(*notes);
    check(!vietnameseOn(engine), "Ctrl+Shift switched all apps to English", "still on");
    keys.tap(*notes, "F12");
    check(realMode.load() != ngosen::Mode::Off, "the cycle key picks a Vietnamese mode", modeName(realMode.load()));
    check(vietnameseOn(engine), "a Vietnamese mode picked turns Vietnamese on", "still off");
    keys.focus(*mail);
    check(realMode.load() != ngosen::Mode::Off, "another app follows the mode picked", modeName(realMode.load()));
    keys.focus(*notes);
    for (int i = 0; i < 3 && realMode.load() != ngosen::Mode::Off; ++i)
        keys.tap(*notes, "F12");
    check(realMode.load() == ngosen::Mode::Off, "the cycle key reaches OFF", modeName(realMode.load()));
    check(vietnameseOn(engine), "OFF from the cycle key leaves the other apps in Vietnamese", "switched off");
    keys.focus(*mail);
    check(realMode.load() != ngosen::Mode::Off, "mail stays in Vietnamese", modeName(realMode.load()));

    if (failures > 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "toggle_scope: all checks passed\n";
    return 0;
}
