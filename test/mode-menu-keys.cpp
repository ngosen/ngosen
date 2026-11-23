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

    std::string label(const fcitx::CommonCandidateList& list, int index) {
        return list.candidateFromAll(index).text().toString();
    }

    std::string modeName(ngosen::Mode mode) {
        switch (mode) {
            case ngosen::Mode::Off: return "Off";
            case ngosen::Mode::Sen: return "Sen";
            case ngosen::Mode::Preedit: return "Preedit";
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
    config.setValueByPath("ToggleScope", "Each app");
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

    // Sen, Preedit, the emoji switch, Off, then the teencode and Vim mode switches (missing from the saved order, so last,
    // in the default order), and "type `".
    press(engine, entry, *context, FcitxKey_grave);
    auto list = menu(*context);
    check(list && list->totalSize() == 7, "menu opens with seven items", list ? std::to_string(list->totalSize()) : "no menu");
    check(list && label(*list, 2) == "   [w] Emoji suggestions: off", "the emoji switch starts off", list ? label(*list, 2) : "no menu");
    check(list && label(*list, 4) == "   [t] Teencode: off", "the teencode switch starts off", list ? label(*list, 4) : "no menu");
    check(list && label(*list, 5) == "   [v] Vim mode: off", "the Vim mode switch starts off", list ? label(*list, 5) : "no menu");
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

    // w turns emoji suggestions on for the app and keeps the typing mode.
    const auto emojiApps = [] {
        std::ifstream file(std::filesystem::temp_directory_path() / "fcitx5-ngosen-mode-menu-keys/config/fcitx5/conf/ngosen-emoji-apps.conf");
        std::string   line;
        std::string   apps;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#')
                apps += line + ";";
        }
        return apps;
    };
    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_w);
    check(!menu(*context), "w closes the menu", "menu still open");
    check(realMode.load() == ngosen::Mode::Preedit, "w keeps the typing mode", modeName(realMode.load()));
    check(emojiApps() == "gedit;", "w saves emoji on for gedit", emojiApps());
    check(context->commits().size() == 1, "w types nothing", std::to_string(context->commits().size()) + " commits");
    press(engine, entry, *context, FcitxKey_grave);
    list = menu(*context);
    check(list && label(*list, 2) == "   [w] Emoji suggestions: on", "the menu shows emoji on", list ? label(*list, 2) : "no menu");
    press(engine, entry, *context, FcitxKey_w);
    check(emojiApps().empty(), "w again turns emoji off", emojiApps());
    {
        fcitx::RawConfig off;
        off.setValueByPath("EmojiSuggestions", "False");
        engine.setConfig(off);
        press(engine, entry, *context, FcitxKey_grave);
        list = menu(*context);
        check(list && list->totalSize() == 6, "with emoji off everywhere the switch leaves the menu", list ? std::to_string(list->totalSize()) : "no menu");
        press(engine, entry, *context, FcitxKey_Escape);
        off.setValueByPath("EmojiSuggestions", "True");
        engine.setConfig(off);
    }

    // t turns teencode on for the app and keeps the typing mode.
    const auto teencodeApps = [] {
        std::ifstream file(std::filesystem::temp_directory_path() / "fcitx5-ngosen-mode-menu-keys/config/fcitx5/conf/ngosen-teencode-apps.conf");
        std::string   line;
        std::string   apps;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#')
                apps += line + ";";
        }
        return apps;
    };
    press(engine, entry, *context, FcitxKey_grave);
    press(engine, entry, *context, FcitxKey_t);
    check(!menu(*context), "t closes the menu", "menu still open");
    check(realMode.load() == ngosen::Mode::Preedit, "t keeps the typing mode", modeName(realMode.load()));
    check(teencodeApps() == "gedit;", "t saves teencode on for gedit", teencodeApps());
    check(context->commits().size() == 1, "t types nothing", std::to_string(context->commits().size()) + " commits");
    press(engine, entry, *context, FcitxKey_grave);
    list = menu(*context);
    check(list && label(*list, 4) == "   [t] Teencode: on", "the menu shows teencode on", list ? label(*list, 4) : "no menu");
    press(engine, entry, *context, FcitxKey_t);
    check(teencodeApps().empty(), "t again turns teencode off", teencodeApps());
    {
        fcitx::RawConfig off;
        off.setValueByPath("AutoNonVnRestore", "False");
        engine.setConfig(off);
        press(engine, entry, *context, FcitxKey_grave);
        list = menu(*context);
        check(list && list->totalSize() == 6, "without auto restore the teencode switch leaves the menu", list ? std::to_string(list->totalSize()) : "no menu");
        press(engine, entry, *context, FcitxKey_Escape);
        off.setValueByPath("AutoNonVnRestore", "True");
        engine.setConfig(off);
    }

    // A window without a name keeps its emoji switch through a settings reload, and loses it when it closes.
    auto unnamed = std::make_unique<TestInputContext>(&testInstance.instance, "gnome-shell");
    unnamed->focusIn();
    fcitx::InputContextEvent unnamedFocus(unnamed.get(), fcitx::EventType::InputContextFocusIn);
    context->focusOut();
    engine.activate(entry, unnamedFocus);
    press(engine, entry, *unnamed, FcitxKey_grave);
    press(engine, entry, *unnamed, FcitxKey_w);
    engine.reloadConfig();
    press(engine, entry, *unnamed, FcitxKey_grave);
    list = menu(*unnamed);
    check(list && label(*list, 2) == "   [w] Emoji suggestions: on", "a reload keeps emoji on for a window without a name", list ? label(*list, 2) : "no menu");
    press(engine, entry, *unnamed, FcitxKey_Escape);
    const void* closed = unnamed.get();
    unnamed->focusOut();
    unnamed.reset();
    auto reopened = std::make_unique<TestInputContext>(&testInstance.instance, "gnome-shell");
    if (reopened.get() == closed) {
        reopened->focusIn();
        fcitx::InputContextEvent reopenedFocus(reopened.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, reopenedFocus);
        press(engine, entry, *reopened, FcitxKey_grave);
        list = menu(*reopened);
        check(list && label(*list, 2) == "   [w] Emoji suggestions: off", "a new window at a closed one's address starts with emoji off", list ? label(*list, 2) : "no menu");
        press(engine, entry, *reopened, FcitxKey_Escape);
    } else {
        std::cerr << "note: the new window got another address, so window was not checked\n";
    }
    reopened->focusOut();
    context->focusIn();
    engine.activate(entry, focus);

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

    // A rule saved under the bare name also holds for the .desktop name, so going back clears that one.
    auto bare = std::make_unique<TestInputContext>(&testInstance.instance, "mousepad");
    bare->focusIn();
    fcitx::InputContextEvent bareFocus(bare.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, bareFocus);
    press(engine, entry, *bare, FcitxKey_grave);
    press(engine, entry, *bare, FcitxKey_1);
    auto desktop = std::make_unique<TestInputContext>(&testInstance.instance, "mousepad.desktop");
    desktop->focusIn();
    fcitx::InputContextEvent desktopFocus(desktop.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, desktopFocus);
    press(engine, entry, *desktop, FcitxKey_grave);
    press(engine, entry, *desktop, FcitxKey_q);
    check(!hasRule(engine, "mousepad"), "the usual mode picked under the .desktop name drops the bare-name rule", "rule kept");
    engine.activate(entry, quietFocus);
    engine.activate(entry, desktopFocus);
    check(realMode.load() == ngosen::Mode::Preedit, "the .desktop name stays on its usual mode", modeName(realMode.load()));

    // With English as the global setting the app is in OFF, so Preedit is not its usual mode.
    fcitx::RawConfig english = config;
    english.setValueByPath("VietnameseOn", "False");
    engine.setConfig(english);
    auto notes = std::make_unique<TestInputContext>(&testInstance.instance, "notes");
    notes->focusIn();
    fcitx::InputContextEvent notesFocus(notes.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, notesFocus);
    press(engine, entry, *notes, FcitxKey_F12);
    press(engine, entry, *notes, FcitxKey_F12);
    engine.activate(entry, quietFocus);
    engine.activate(entry, notesFocus);
    check(realMode.load() == ngosen::Mode::Preedit, "with English everywhere, the cycle key's Preedit holds for the app", modeName(realMode.load()));
    check(hasRule(engine, "notes"), "with English everywhere, Preedit is kept for the app", "no rule");

    // A key the menu does not use closes it and is typed; a modifier on its own leaves it open.
    notes->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    press(engine, entry, *notes, FcitxKey_grave);
    press(engine, entry, *notes, FcitxKey_Shift_L);
    check(menu(*notes) != nullptr, "a modifier on its own keeps the menu", "menu closed");
    press(engine, entry, *notes, FcitxKey_b);
    check(!menu(*notes), "a key the menu does not use closes it", "menu still open");
    const auto typed = notes->inputPanel().clientPreedit().toString();
    check(typed == "b", "the key that closed the menu is typed", typed);

    // A hotkey with a modifier types nothing, so pressing it again is the way to close the menu.
    {
        fcitx::RawConfig ctrlGrave;
        ctrlGrave.setValueByPath("ModeMenuKey/0", "Control+grave");
        engine.setConfig(ctrlGrave);
        const auto pressCtrlGrave = [&] {
            const fcitx::Key key(FcitxKey_grave, fcitx::KeyState::Ctrl);
            fcitx::KeyEvent  down(notes.get(), key, false);
            engine.keyEvent(entry, down);
            fcitx::KeyEvent up(notes.get(), key, true);
            engine.keyEvent(entry, up);
        };
        pressCtrlGrave();
        check(menu(*notes) != nullptr, "Ctrl+` opens the menu", "no menu");
        pressCtrlGrave();
        check(!menu(*notes), "Ctrl+` again closes the menu", "menu still open");
        ctrlGrave.setValueByPath("ModeMenuKey/0", "grave");
        engine.setConfig(ctrlGrave);
    }

    // The notice of a mode change must not clear a menu opened while it shows.
    press(engine, entry, *notes, FcitxKey_grave);
    press(engine, entry, *notes, FcitxKey_q);
    press(engine, entry, *notes, FcitxKey_grave);
    pumpEventLoop(testInstance.instance, 1000);
    check(menu(*notes) != nullptr, "a menu opened right after a mode change stays", "menu closed");
    press(engine, entry, *notes, FcitxKey_Escape);

    // The wait of a menu already closed leaves what is typed after it alone.
    press(engine, entry, *notes, FcitxKey_grave);
    press(engine, entry, *notes, FcitxKey_Escape);
    press(engine, entry, *notes, FcitxKey_b);
    pumpEventLoop(testInstance.instance, 5300);
    const auto kept = notes->inputPanel().clientPreedit().toString();
    check(kept == "b", "the wait of a closed menu keeps the text typed after it", kept);
    press(engine, entry, *notes, FcitxKey_Escape);

    // A menu nobody uses closes by itself; moving in it starts the wait again.
    press(engine, entry, *notes, FcitxKey_grave);
    pumpEventLoop(testInstance.instance, 5300);
    check(!menu(*notes), "a menu left alone closes by itself", "menu still open");
    press(engine, entry, *notes, FcitxKey_grave);
    pumpEventLoop(testInstance.instance, 3000);
    press(engine, entry, *notes, FcitxKey_Down);
    pumpEventLoop(testInstance.instance, 3000);
    check(menu(*notes) != nullptr, "moving in the menu keeps it open", "menu closed");
    pumpEventLoop(testInstance.instance, 2500);
    check(!menu(*notes), "an unused menu closes by itself", "menu still open");
    check(realMode.load() == ngosen::Mode::Preedit, "closing by itself keeps the mode", modeName(realMode.load()));

    // Each click case gets its own field, so one failing leaves the next alone.
    engine.setConfig(config);
    auto openField = [&](const std::string& app) {
        auto field = std::make_unique<TestInputContext>(&testInstance.instance, app);
        field->setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit, fcitx::CapabilityFlag::SurroundingText});
        field->focusIn();
        fcitx::InputContextEvent fieldFocus(field.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, fieldFocus);
        return field;
    };

    // A click in the field: the caret moves with no edit.
    auto clickedField = openField("clickedfield");
    clickedField->surroundingText().setText("abc def", 7, 7);
    clickedField->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 1);
    press(engine, entry, *clickedField, FcitxKey_grave);
    check(menu(*clickedField) != nullptr, "the menu opens in clickedField", "no menu");
    clickedField->surroundingText().setText("abc def", 2, 2);
    clickedField->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 1);
    check(!menu(*clickedField), "a click in the field closes the menu", "menu still open");
    clickedField.reset();

    // Some apps answer a click with a report that still has the old caret.
    auto reportField = openField("reportfield");
    reportField->surroundingText().setText("abc", 3, 3);
    reportField->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 1);
    press(engine, entry, *reportField, FcitxKey_grave);
    check(menu(*reportField) != nullptr, "the menu opens in reportField", "no menu");
    reportField->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 200);
    check(menu(*reportField) != nullptr, "a report right after the menu opens keeps it", "menu closed");
    pumpEventLoop(testInstance.instance, 100);
    press(engine, entry, *reportField, FcitxKey_Down);
    reportField->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 200);
    check(menu(*reportField) != nullptr, "a report right after a key in the menu keeps it", "menu closed");
    pumpEventLoop(testInstance.instance, 300);
    reportField->updateSurroundingText();
    pumpEventLoop(testInstance.instance, 200);
    check(!menu(*reportField), "a report long after the menu opened closes it", "menu still open");
    reportField.reset();

    // KWin sends a report just before each key.
    auto keyField = openField("keyfield");
    press(engine, entry, *keyField, FcitxKey_grave);
    press(engine, entry, *keyField, FcitxKey_q);
    check(realMode.load() == ngosen::Mode::Preedit, "q picks Preedit in keyField", modeName(realMode.load()));
    press(engine, entry, *keyField, FcitxKey_grave);
    pumpEventLoop(testInstance.instance, 300);
    keyField->updateSurroundingText();
    press(engine, entry, *keyField, FcitxKey_Down);
    pumpEventLoop(testInstance.instance, 200);
    check(menu(*keyField) != nullptr, "a key right after a late report keeps the menu", "menu closed");
    pumpEventLoop(testInstance.instance, 300);
    keyField->updateSurroundingText();
    press(engine, entry, *keyField, FcitxKey_1);
    check(realMode.load() == ngosen::Mode::Sen, "a key right after a late report reaches the menu", modeName(realMode.load()));
    keyField.reset();

    // Releasing a key held across the click is not a key for the menu.
    auto releaseField = openField("releasefield");
    press(engine, entry, *releaseField, FcitxKey_grave);
    check(menu(*releaseField) != nullptr, "the menu opens in releaseField", "no menu");
    pumpEventLoop(testInstance.instance, 300);
    releaseField->updateSurroundingText();
    fcitx::KeyEvent ctrlUp(releaseField.get(), fcitx::Key(FcitxKey_Control_L), true);
    engine.keyEvent(entry, ctrlUp);
    pumpEventLoop(testInstance.instance, 200);
    check(!menu(*releaseField), "a key release after a late report leaves the close", "menu still open");
    releaseField.reset();

    // A click X11 reports elsewhere, such as in another field of the same window.
    auto clickedAway = openField("clickedaway");
    press(engine, entry, *clickedAway, FcitxKey_grave);
    check(menu(*clickedAway) != nullptr, "the menu opens in clickedAway", "no menu");
    engine.pointerClicked();
    pumpEventLoop(testInstance.instance, 200);
    check(!menu(*clickedAway), "a click outside the field closes the menu", "menu still open");
    clickedAway.reset();

    // A click just before the menu opens is not a click away from it.
    auto clickedBefore = openField("clickedbefore");
    engine.pointerClicked();
    press(engine, entry, *clickedBefore, FcitxKey_grave);
    pumpEventLoop(testInstance.instance, 200);
    check(menu(*clickedBefore) != nullptr, "an earlier click leaves a menu opened after it", "menu closed");
    press(engine, entry, *clickedBefore, FcitxKey_Escape);
    press(engine, entry, *clickedBefore, FcitxKey_grave);
    engine.pointerClicked();
    press(engine, entry, *clickedBefore, FcitxKey_Escape);
    press(engine, entry, *clickedBefore, FcitxKey_grave);
    pumpEventLoop(testInstance.instance, 200);
    check(menu(*clickedBefore) != nullptr, "a click before reopening leaves the new menu", "menu closed");
    clickedBefore.reset();

    // A click in some apps comes as a reset of the field; in Sen the menu must not stay drawn.
    auto resetField = openField("resetfield");
    press(engine, entry, *resetField, FcitxKey_grave);
    press(engine, entry, *resetField, FcitxKey_1);
    press(engine, entry, *resetField, FcitxKey_grave);
    check(menu(*resetField) != nullptr, "the menu opens in resetField", "no menu");
    int                      panelRedraws = 0;
    auto                     redrawWatch  = testInstance.instance.watchEvent(fcitx::EventType::InputContextUpdateUI, fcitx::EventWatcherPhase::Default, [&](fcitx::Event& event) {
        auto& update = static_cast<fcitx::InputContextUpdateUIEvent&>(event);
        if (update.inputContext() == resetField.get() && update.component() == fcitx::UserInterfaceComponent::InputPanel)
            ++panelRedraws;
    });
    fcitx::InputContextEvent appReset(resetField.get(), fcitx::EventType::InputContextReset);
    engine.reset(entry, appReset);
    check(!menu(*resetField), "an app reset closes the menu", "menu still open");
    check(panelRedraws > 0, "an app reset redraws the closed menu", "no redraw");
    redrawWatch.reset();
    resetField.reset();

    // The panel picks from the menu it shows when the button goes down.
    auto clickedMenu = openField("clickedmenu");
    press(engine, entry, *clickedMenu, FcitxKey_grave);
    check(menu(*clickedMenu) != nullptr, "the menu opens in clickedMenu", "no menu");
    engine.pointerClicked();
    if (auto shown = menu(*clickedMenu))
        shown->candidateFromAll(0).select(clickedMenu.get());
    pumpEventLoop(testInstance.instance, 200);
    check(realMode.load() == ngosen::Mode::Sen, "a click on a menu item picks it", modeName(realMode.load()));
    clickedMenu.reset();

    check(!fcitx::modeMenuCanDraw("classicui", "ibus", "wayland:", false), "a field relayed on Wayland has no panel", "true");
    check(fcitx::modeMenuCanDraw("classicui", "ibus", "x11::0", false), "a relayed field on X11 can draw", "false");
    check(fcitx::modeMenuCanDraw("classicui", "dbus", "wayland:", false), "another frontend can draw", "false");
    check(fcitx::modeMenuCanDraw("kimpanel", "ibus", "wayland:", false), "the panel extension draws for any field", "false");
    check(fcitx::modeMenuCanDraw("classicui", "ibus", "wayland:", true), "a client that draws its own panel can draw", "false");

    // With a label, GNOME's kimpanel shows text in the top bar instead of the Ngó Sen icon.
    for (const auto mode : {ngosen::Mode::Sen, ngosen::Mode::Off}) {
        realMode         = mode;
        const auto label = engine.subModeLabel(entry, *context);
        check(label.empty(), "no panel label in " + modeName(mode), label);
        // The switch popup still has to tell Sen and Off apart.
        const auto subMode  = engine.subMode(entry, *context);
        const auto expected = mode == ngosen::Mode::Sen ? "Telex" : "OFF";
        check(subMode == expected, "switch popup names " + modeName(mode), subMode);
    }

    return failures == 0 ? 0 : 1;
}
