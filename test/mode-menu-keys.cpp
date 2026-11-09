// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mode menu and the cycle key, driven by key presses: open the menu, move in it, pick a mode by
// its shortcut, close it with Escape, type the hotkey's own character, and step to the next mode.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
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

    bool hasRule(const fcitx::NgoSenEngine& engine, const std::string& app) {
        fcitx::RawConfig raw;
        engine.getSubConfig("app_rules")->save(raw);
        bool found = false;
        if (auto rules = raw.get("Rules")) {
            rules->visitSubItems([&](fcitx::RawConfig& rule, const std::string&) {
                auto name = rule.get("App");
                found     = found || (name && name->value() == app);
                return true;
            });
        }
        return found;
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
    // Saved before Default Typing left the menu.
    config.setValueByPath("ModeOrder", "Sen,Preedit,Emoji,Off,Default");
    config.setValueByPath("ShowModeDefault", "True");
    config.setValueByPath("ShortcutDefault", "r");
    engine.setConfig(config);

    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "gedit");
    context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    // Sen, Preedit, Emoji, Off, and "type `".
    press(engine, entry, *context, FcitxKey_grave);
    auto list = menu(*context);
    check(list && list->totalSize() == 5, "menu opens with five items", list ? std::to_string(list->totalSize()) : "no menu");
    // gedit has no rule of its own, so it follows the global Preedit.
    check(list && list->globalCursorIndex() == 1, "the app's mode is highlighted", list ? std::to_string(list->globalCursorIndex()) : "no menu");
    press(engine, entry, *context, FcitxKey_Down);
    check(list && list->globalCursorIndex() == 2, "Down moves the highlight", list ? std::to_string(list->globalCursorIndex()) : "no menu");

    press(engine, entry, *context, FcitxKey_1);
    check(!menu(*context), "picking a mode closes the menu", "menu still open");
    list.reset();
    check(realMode.load() == ngosen::Mode::Sen, "shortcut 1 picks Sen", modeName(realMode.load()));
    check(hasRule(engine, "gedit"), "a mode the app does not use anyway is kept for it", "no rule");

    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_q);
    check(realMode.load() == ngosen::Mode::Preedit, "shortcut q picks Preedit", modeName(realMode.load()));
    check(!hasRule(engine, "gedit"), "picking the app's usual mode drops its rule", "rule kept");
    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_1);

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
    check(!hasRule(engine, "gedit"), "cycling to the app's usual mode drops its rule", "rule kept");

    // A field that reports no app of its own, away from GNOME Shell's frontend.
    auto relayed = std::make_unique<TestInputContext>(&testInstance.instance, "gnome-shell");
    relayed->focusIn();
    fcitx::InputContextEvent relayedFocus(relayed.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, relayedFocus);
    press(engine, entry, *relayed, FcitxKey_grave);
    const auto relayedApp = relayed->inputPanel().auxDown().toString();
    check(relayedApp == "App: this window", "a window with its own field is shown as this window", relayedApp);
    press(engine, entry, *relayed, FcitxKey_Escape);
    relayed->focusOut();
    context->focusIn();
    engine.activate(entry, focus);
    press(engine, entry, *context, FcitxKey_grave);
    const auto namedApp = context->inputPanel().auxDown().toString();
    check(namedApp == "App: gedit", "a named app is shown by name", namedApp);
    press(engine, entry, *context, FcitxKey_Escape);

    // GNOME Shell relays every app through one field, so a pick holds for all of them.
    context->focusOut();
    auto shellField = std::make_unique<TestInputContext>(&testInstance.instance, "", "ibus");
    shellField->focusIn();
    fcitx::InputContextEvent shellFocus(shellField.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, shellFocus);
    press(engine, entry, *shellField, FcitxKey_grave);
    const auto shellApp = shellField->inputPanel().auxDown().toString();
    check(shellApp == "App: apps with no name (the Ngó Sen extension can tell them apart)", "a field GNOME Shell relays points to the extension", shellApp);
    press(engine, entry, *shellField, FcitxKey_Escape);

    // The menu shows the app it opened for, so a name that comes after it closes it.
    engine.shellFocusedAppChanged("org.gnome.TextEditor.desktop");
    press(engine, entry, *shellField, FcitxKey_grave);
    check(menu(*shellField) != nullptr, "the menu opens for the app GNOME Shell names", "no menu");
    engine.shellFocusedAppChanged("org.gnome.TextEditor.desktop");
    check(menu(*shellField) != nullptr, "the same name again keeps the menu", "menu closed");
    engine.shellFocusedAppChanged("org.gnome.Nautilus.desktop");
    check(!menu(*shellField), "another app's name closes the menu", "menu still open");
    press(engine, entry, *shellField, FcitxKey_1);
    check(!hasRule(engine, "org.gnome.TextEditor.desktop") && !hasRule(engine, "org.gnome.Nautilus.desktop"), "a key after the name changed picks nothing", "a rule was saved");
    engine.shellFocusedAppChanged("");
    shellField->focusOut();

    // A field with its own name keeps its menu whatever GNOME Shell names.
    context->focusIn();
    engine.activate(entry, focus);
    press(engine, entry, *context, FcitxKey_grave);
    engine.shellFocusedAppChanged("org.gnome.Nautilus.desktop");
    check(menu(*context) != nullptr, "a named field keeps its menu when GNOME Shell names an app", "menu closed");
    press(engine, entry, *context, FcitxKey_Escape);
    engine.shellFocusedAppChanged("");
    context->focusOut();

    // Leaving the window closes the menu, so later keys are not taken as picks for the app left behind.
    auto writer = std::make_unique<TestInputContext>(&testInstance.instance, "writer");
    writer->focusIn();
    fcitx::InputContextEvent writerFocus(writer.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, writerFocus);
    press(engine, entry, *writer, FcitxKey_grave);
    fcitx::InputContextEvent writerLeft(writer.get(), fcitx::EventType::InputContextFocusOut);
    engine.deactivate(entry, writerLeft);
    check(!menu(*writer), "leaving the window closes the menu", "menu still open");
    engine.activate(entry, writerFocus);
    press(engine, entry, *writer, FcitxKey_e);
    check(!hasRule(engine, "writer"), "a key after leaving picks nothing", "writer got a rule");

    // Keys that reach another field never pick for the app that opened the menu.
    auto reader = std::make_unique<TestInputContext>(&testInstance.instance, "reader");
    reader->focusIn();
    fcitx::InputContextEvent readerFocus(reader.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, readerFocus);
    press(engine, entry, *reader, FcitxKey_grave);
    auto other = std::make_unique<TestInputContext>(&testInstance.instance, "kate");
    other->focusIn();
    press(engine, entry, *other, FcitxKey_e);
    check(!hasRule(engine, "reader"), "a key in another app picks nothing for the first", "reader got a rule");
    check(realMode.load() != ngosen::Mode::Off, "a key in another app keeps the mode", modeName(realMode.load()));

    // Anything clearing the panel closes the menu, so no key picks from a menu nobody sees.
    auto cleared = std::make_unique<TestInputContext>(&testInstance.instance, "cleared");
    cleared->focusIn();
    fcitx::InputContextEvent clearedFocus(cleared.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, clearedFocus);
    press(engine, entry, *cleared, FcitxKey_grave);
    cleared->inputPanel().reset();
    press(engine, entry, *cleared, FcitxKey_e);
    check(!hasRule(engine, "cleared"), "a key after the panel was cleared picks nothing", "cleared got a rule");

    // A click reported by X11 closes the menu.
    engine.activate(entry, clearedFocus);
    press(engine, entry, *cleared, FcitxKey_grave);
    g_mouse_clicked.store(true, std::memory_order_release);
    press(engine, entry, *cleared, FcitxKey_a);
    check(!menu(*cleared), "a click closes the menu", "menu still open");

    // A window closed with its menu open leaves nothing for later keys to pick for.
    auto closing = std::make_unique<TestInputContext>(&testInstance.instance, "closing");
    closing->focusIn();
    fcitx::InputContextEvent closingFocus(closing.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, closingFocus);
    press(engine, entry, *closing, FcitxKey_grave);
    check(menu(*closing) != nullptr, "the menu opens in the window about to close", "no menu");
    closing.reset();
    press(engine, entry, *other, FcitxKey_e);
    check(!hasRule(engine, "closing"), "a key after the window closed picks nothing", "closing got a rule");

    // OFF stays pinned: dropped, it would follow the switch key back to Vietnamese.
    auto quiet = std::make_unique<TestInputContext>(&testInstance.instance, "quiet");
    quiet->focusIn();
    fcitx::InputContextEvent quietFocus(quiet.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, quietFocus);
    press(engine, entry, *quiet, FcitxKey_grave);
    press(engine, entry, *quiet, FcitxKey_e);
    check(realMode.load() == ngosen::Mode::Off, "shortcut e picks OFF", modeName(realMode.load()));
    check(hasRule(engine, "quiet"), "OFF is kept for the app", "no rule");

    check(!fcitx::modeMenuCanDraw("classicui", "ibus", "wayland:", false), "a field relayed on Wayland has no panel", "true");
    check(fcitx::modeMenuCanDraw("classicui", "ibus", "x11::0", false), "a relayed field on X11 can draw", "false");
    check(fcitx::modeMenuCanDraw("classicui", "dbus", "wayland:", false), "another frontend can draw", "false");
    check(fcitx::modeMenuCanDraw("kimpanel", "ibus", "wayland:", false), "the panel extension draws for any field", "false");
    check(fcitx::modeMenuCanDraw("classicui", "ibus", "wayland:", true), "a client that draws its own panel can draw", "false");

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
