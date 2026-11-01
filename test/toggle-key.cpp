// SPDX-License-Identifier: GPL-3.0-or-later
//
// The toggle key switches between English and Vietnamese in the app it is pressed in, which remembers
// that; an app it was never pressed in follows the last press. English lets keys through untouched.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <fcitx/candidatelist.h>

#include <chrono>
#include <map>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

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
            case ngosen::Mode::Emoji: return "Emoji";
        }
        return "?";
    }

    struct Keys {
        fcitx::NgoSenEngine&           engine;
        const fcitx::InputMethodEntry& entry;

        // Returns whether the engine kept the key from the app.
        bool send(TestInputContext& context, fcitx::Key key, bool release) {
            fcitx::KeyEvent event(&context, key, release);
            engine.keyEvent(entry, event);
            return event.filtered();
        }

        bool tap(TestInputContext& context, const char* key) {
            const bool kept = send(context, fcitx::Key(key), false);
            send(context, fcitx::Key(key), true);
            return kept;
        }

        // Ctrl held, Shift pressed and released, then Ctrl released, with `between` typed while both are down.
        void ctrlShift(TestInputContext& context, fcitx::KeySym between = FcitxKey_None) {
            const fcitx::KeyStates ctrl{fcitx::KeyState::Ctrl};
            const fcitx::KeyStates both{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift};
            send(context, fcitx::Key(FcitxKey_Control_L), false);
            send(context, fcitx::Key(FcitxKey_Shift_L, ctrl), false);
            if (between != FcitxKey_None) {
                send(context, fcitx::Key(between, both), false);
                send(context, fcitx::Key(between, both), true);
            }
            send(context, fcitx::Key(FcitxKey_Shift_L, both), true);
            send(context, fcitx::Key(FcitxKey_Control_L, ctrl), true);
        }

        void focus(TestInputContext& context) {
            context.focusIn();
            fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, event);
        }
    };

    std::string savedConfig() {
        std::ifstream     file(std::string(getenv("XDG_CONFIG_HOME")) + "/fcitx5/conf/ngosen.conf");
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    bool menuOpen(TestInputContext& context) {
        auto list = context.inputPanel().candidateList();
        return list && list->size() > 1;
    }

    // App name to "True" or "False", as the engine saved them.
    std::map<std::string, std::string> savedSwitches() {
        std::ifstream                      file(std::string(getenv("XDG_CONFIG_HOME")) + "/fcitx5/conf/ngosen-app-switch.conf");
        std::map<std::string, std::string> switches;
        std::string                        line;
        std::string                        app;
        while (std::getline(file, line)) {
            // fcitx5 writes a value equal to the default as a comment.
            if (line.rfind("# ", 0) == 0)
                line = line.substr(2);
            if (line.rfind("App=", 0) == 0)
                app = line.substr(4);
            else if (line.rfind("Vietnamese=", 0) == 0)
                switches[app] = line.substr(11);
        }
        return switches;
    }

    std::string describe(const std::map<std::string, std::string>& switches) {
        std::string text;
        for (const auto& [app, on] : switches)
            text += app + "=" + on + " ";
        return text.empty() ? "(none)" : text;
    }

    std::string savedRule(const fcitx::NgoSenEngine& engine, const std::string& name) {
        fcitx::RawConfig raw;
        engine.getSubConfig("app_rules")->save(raw);
        std::string mode = "(none)";
        if (auto rules = raw.get("Rules")) {
            rules->visitSubItems([&](fcitx::RawConfig& rule, const std::string&) {
                auto app = rule.get("App");
                if (app && app->value() == name)
                    mode = rule.valueByPath("Mode") ? *rule.valueByPath("Mode") : "?";
                return true;
            });
        }
        return mode;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-toggle-key");
    TestInstance            testInstance;
    fcitx::NgoSenEngine     engine(&testInstance.instance);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    Keys                    keys{engine, entry};

    auto                    gedit = std::make_unique<TestInputContext>(&testInstance.instance, "gedit");
    auto                    kate  = std::make_unique<TestInputContext>(&testInstance.instance, "kate");
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Off, "a new install starts in English", modeName(realMode.load()));

    // English is as if no input method were there.
    check(!keys.tap(*gedit, "a"), "English lets letters through", "kept");
    check(!keys.tap(*gedit, "Control+c"), "English lets Ctrl+C through", "kept");
    check(!keys.tap(*gedit, "Control+space"), "Ctrl+Space is not the key on a new install", "kept");
    check(gedit->commits().empty(), "English commits nothing", std::to_string(gedit->commits().size()) + " commits");
    check(!keys.tap(*gedit, "grave"), "English lets ` through", "kept");
    check(!menuOpen(*gedit), "` opens no menu in English", "menu open");

    keys.ctrlShift(*gedit, FcitxKey_T);
    check(realMode.load() == ngosen::Mode::Off, "Ctrl+Shift+T does not switch", modeName(realMode.load()));
    const fcitx::KeyStates ctrl{fcitx::KeyState::Ctrl};
    const fcitx::KeyStates both{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift};
    keys.send(*gedit, fcitx::Key(FcitxKey_Control_L), false);
    keys.send(*gedit, fcitx::Key(FcitxKey_Shift_L, ctrl), false);
    keys.send(*gedit, fcitx::Key(FcitxKey_Shift_L, both), true);
    check(realMode.load() == ngosen::Mode::Off, "Ctrl+Shift waits for Ctrl to come up", modeName(realMode.load()));
    keys.send(*gedit, fcitx::Key(FcitxKey_Control_L, ctrl), true);
    check(realMode.load() == ngosen::Mode::Sen, "Ctrl+Shift switches to Vietnamese", modeName(realMode.load()));
    check(savedConfig().find("VietnameseOn=True") != std::string::npos, "the choice is saved", savedConfig());

    // fcitx5 leaves the default toggle key out of the file the engine saved.
    engine.reloadConfig();
    keys.focus(*gedit);
    check(!keys.tap(*gedit, "Control+space"), "Ctrl+Space stays unused after a reload", "kept");
    const auto beforeReloadSwitch = realMode.load();
    keys.ctrlShift(*gedit);
    check(realMode.load() != beforeReloadSwitch, "Ctrl+Shift still switches after a reload", modeName(realMode.load()));
    keys.ctrlShift(*gedit);

    // An app never switched in follows the last switch; one switched in keeps its own.
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Sen, "another app follows the last switch", modeName(realMode.load()));
    keys.ctrlShift(*kate);
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "gedit keeps its own switch", modeName(realMode.load()));

    // A window without a name is remembered while it is open, and never saved.
    auto unnamed = std::make_unique<TestInputContext>(&testInstance.instance, "");
    keys.focus(*unnamed);
    check(realMode.load() == ngosen::Mode::Off, "an unnamed window follows the last switch", modeName(realMode.load()));
    keys.ctrlShift(*unnamed);
    const auto switches = savedSwitches();
    check(switches.size() == 2 && switches.count("gedit") && switches.at("gedit") == "True" && switches.count("kate") && switches.at("kate") == "False",
          "each named app's switch is saved", describe(switches));

    // A rule picked from the menu wins over the switch, until the toggle key is pressed there again.
    keys.focus(*gedit);
    keys.tap(*gedit, "grave");
    check(menuOpen(*gedit), "` opens the menu in Vietnamese", "no menu");
    keys.tap(*gedit, "e");
    check(realMode.load() == ngosen::Mode::Off, "the menu sets Off for gedit", modeName(realMode.load()));
    keys.ctrlShift(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "the toggle wins over the menu's Off", modeName(realMode.load()));
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Off, "kate keeps English", modeName(realMode.load()));
    keys.ctrlShift(*kate);
    check(realMode.load() == ngosen::Mode::Sen, "kate switches back to Vietnamese", modeName(realMode.load()));
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "gedit keeps Vietnamese", modeName(realMode.load()));

    // The unnamed window keeps English through a reload while the last switch elsewhere was Vietnamese.
    keys.focus(*unnamed);
    keys.ctrlShift(*unnamed);
    keys.focus(*gedit);
    keys.ctrlShift(*gedit);
    keys.ctrlShift(*gedit);
    engine.reloadConfig();
    keys.focus(*unnamed);
    check(realMode.load() == ngosen::Mode::Off, "an unnamed window keeps its switch through a reload", modeName(realMode.load()));
    const std::string unnamedName = fcitx::NgoSenEngine::getProgramName(unnamed.get());
    unnamed.reset();
    check(engine.modeFor(unnamedName) == ngosen::Mode::Sen, "a closed window's switch is forgotten", modeName(engine.modeFor(unnamedName)));

    // Switching off and on keeps the Preedit the menu picked for gedit.
    keys.focus(*gedit);
    keys.tap(*gedit, "grave");
    keys.tap(*gedit, "q");
    check(realMode.load() == ngosen::Mode::Preedit, "the menu sets Preedit for gedit", modeName(realMode.load()));
    keys.ctrlShift(*gedit);
    keys.ctrlShift(*gedit);
    check(realMode.load() == ngosen::Mode::Preedit, "the toggle key keeps gedit's Preedit", modeName(realMode.load()));
    check(savedRule(engine, "gedit") == "5", "the toggle key leaves gedit's rule alone", savedRule(engine, "gedit"));

    // A new app follows whichever way the last switch went.
    auto chat = std::make_unique<TestInputContext>(&testInstance.instance, "chat");
    keys.focus(*chat);
    check(realMode.load() == ngosen::Mode::Sen, "a new app follows the last switch to Vietnamese", modeName(realMode.load()));
    keys.focus(*kate);
    keys.ctrlShift(*kate);
    auto news = std::make_unique<TestInputContext>(&testInstance.instance, "news");
    keys.focus(*news);
    check(realMode.load() == ngosen::Mode::Off, "a new app follows the last switch to English", modeName(realMode.load()));

    // Picking from the menu forgets the app's switch, so the pick holds and Default follows the last switch.
    keys.focus(*kate);
    keys.ctrlShift(*kate);
    keys.tap(*kate, "grave");
    keys.tap(*kate, "e");
    keys.focus(*gedit);
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Off, "the menu's Off holds after the toggle key", modeName(realMode.load()));
    keys.ctrlShift(*kate);
    keys.tap(*kate, "grave");
    check(menuOpen(*kate), "` opens the menu in kate again", "no menu");
    keys.tap(*kate, "r");
    keys.focus(*chat);
    keys.ctrlShift(*chat);
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Off, "Default makes kate follow the last switch", modeName(realMode.load()));

    // "firefox.desktop" falls back to firefox's switch, but one of its own wins.
    auto firefox        = std::make_unique<TestInputContext>(&testInstance.instance, "firefox");
    auto firefoxDesktop = std::make_unique<TestInputContext>(&testInstance.instance, "firefox.desktop");
    keys.focus(*firefox);
    keys.ctrlShift(*firefox);
    keys.focus(*chat);
    keys.ctrlShift(*chat);
    keys.ctrlShift(*chat);
    keys.focus(*firefoxDesktop);
    check(realMode.load() == ngosen::Mode::Sen, "firefox.desktop follows firefox's switch", modeName(realMode.load()));
    keys.ctrlShift(*firefoxDesktop);
    keys.focus(*chat);
    keys.ctrlShift(*chat);
    keys.focus(*firefoxDesktop);
    check(realMode.load() == ngosen::Mode::Off, "firefox.desktop keeps a switch of its own", modeName(realMode.load()));
    keys.focus(*firefox);
    check(realMode.load() == ngosen::Mode::Sen, "firefox keeps its own switch", modeName(realMode.load()));

    // A pick from the menu is not undone by a switch saved under the app's other name.
    keys.ctrlShift(*firefox);
    keys.focus(*firefoxDesktop);
    keys.ctrlShift(*firefoxDesktop);
    keys.tap(*firefoxDesktop, "grave");
    keys.tap(*firefoxDesktop, "q");
    keys.focus(*firefox);
    keys.focus(*firefoxDesktop);
    check(realMode.load() == ngosen::Mode::Preedit, "a menu pick holds over the other name's switch", modeName(realMode.load()));

    // Defaults also forgets the switch of an open window without a name.
    auto unnamedOpen = std::make_unique<TestInputContext>(&testInstance.instance, "");
    keys.focus(*unnamedOpen);
    keys.ctrlShift(*unnamedOpen);
    const bool unnamedOn = realMode.load() != ngosen::Mode::Off;
    keys.focus(*chat);
    keys.ctrlShift(*chat);
    if ((realMode.load() != ngosen::Mode::Off) == unnamedOn)
        keys.ctrlShift(*chat);
    const std::string unnamedOpenName = fcitx::NgoSenEngine::getProgramName(unnamedOpen.get());

    // What follows checks the older settings, not the switches.
    fcitx::RawConfig noSwitches;
    noSwitches.get("Switches", true);
    engine.setSubConfig("app_switches", noSwitches);
    check((engine.modeFor(unnamedOpenName) != ngosen::Mode::Off) != unnamedOn, "Defaults forgets an unnamed window's switch", modeName(engine.modeFor(unnamedOpenName)));

    // Settings saved before the toggle key existed keep the key fcitx5 used to switch with.
    {
        std::ofstream file(std::string(getenv("XDG_CONFIG_HOME")) + "/fcitx5/conf/ngosen.conf");
        file << "Mode=Sen\nInputMethod=Telex\n";
    }
    engine.reloadConfig();
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Sen, "older settings start in Vietnamese", modeName(realMode.load()));
    const auto before = realMode.load();
    check(keys.tap(*kate, "Control+space"), "older settings switch with Ctrl+Space", "passed to the app");
    check(realMode.load() != before, "Ctrl+Space switches with older settings", modeName(realMode.load()));
    keys.ctrlShift(*kate);
    check(realMode.load() != before, "Ctrl+Shift does not switch with older settings", modeName(realMode.load()));

    {
        std::ofstream file(std::string(getenv("XDG_CONFIG_HOME")) + "/fcitx5/conf/ngosen.conf");
        file << "Mode=OFF\n";
    }
    engine.reloadConfig();
    auto mail = std::make_unique<TestInputContext>(&testInstance.instance, "mail");
    keys.focus(*mail);
    check(realMode.load() == ngosen::Mode::Off, "older settings with OFF as the mode start in English", modeName(realMode.load()));

    // The word being typed is finished in the mode it was typed in. A Ctrl press commits the preedit
    // anyway, so the key here is one without modifiers.
    fcitx::RawConfig preedit;
    preedit.setValueByPath("Mode", "Preedit");
    preedit.setValueByPath("VietnameseOn", "True");
    preedit.setValueByPath("ToggleKey/0", "F12");
    preedit.setValueByPath("ToggleKey/1", "Control+Shift_L");
    engine.setConfig(preedit);
    auto notes = std::make_unique<TestInputContext>(&testInstance.instance, "notes");
    notes->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    keys.focus(*notes);
    for (const char* key : {"v", "i", "e", "e", "t", "j"})
        keys.tap(*notes, key);
    keys.tap(*notes, "F12");
    const std::string committed = notes->commits().empty() ? "(nothing)" : notes->commits().back();
    check(committed == "việt", "switching to English commits the word in preedit", committed);

#if NGOSEN_USE_MODERN_FCITX_API
    // Held past fcitx5's limit for modifier-only keys, as around a Ctrl+Shift+click.
    fcitx::RawConfig limit;
    limit.setValueByPath("Hotkey/ModifierOnlyKeyTimeout", "100");
    testInstance.instance.globalConfig().load(limit, true);
    keys.send(*notes, fcitx::Key(FcitxKey_Control_L), false);
    keys.send(*notes, fcitx::Key(FcitxKey_Shift_L, fcitx::KeyStates{fcitx::KeyState::Ctrl}), false);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // A held key repeats, and the repeat must not restart the hold.
    keys.send(*notes, fcitx::Key(FcitxKey_Shift_L, fcitx::KeyStates{fcitx::KeyState::Ctrl}), false);
    keys.send(*notes, fcitx::Key(FcitxKey_Shift_L, fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift}), true);
    check(realMode.load() == ngosen::Mode::Off, "a long Ctrl+Shift hold does not switch", modeName(realMode.load()));
#endif

    // Leaving a password field or changing the group is not a pick of Vietnamese.
    using Reason = fcitx::InputMethodSwitchedReason;
    check(fcitx::switchPickedByUser(Reason::Trigger), "the trigger key picks Ngó Sen", "not a pick");
    check(fcitx::switchPickedByUser(Reason::Other), "the tray picks Ngó Sen", "not a pick");
    check(!fcitx::switchPickedByUser(Reason::CapabilityChanged), "leaving a password field picks nothing", "a pick");
    check(!fcitx::switchPickedByUser(Reason::GroupChange), "changing the group picks nothing", "a pick");

    return failures == 0 ? 0 : 1;
}
