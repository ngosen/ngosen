// SPDX-License-Identifier: GPL-3.0-or-later
//
// ToggleVietnamese is what a desktop shortcut runs: it switches like the toggle key, also when no field
// is focused, and the shortcut's own modifiers must not switch a second time when they come up.
#include "ngosen-dbus-object.h"
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

        // One field has focus at a time, as on a desktop.
        TestInputContext* focused = nullptr;

        void              focus(TestInputContext& context) {
            if (focused != nullptr && focused != &context)
                leave(*focused);
            focused = &context;
            context.focusIn();
            fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, event);
        }

        void leave(TestInputContext& context) {
            fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusOut);
            engine.deactivate(entry, event);
            context.focusOut();
            focused = nullptr;
        }
    };

    bool menuOpen(TestInputContext& context) {
        auto list = context.inputPanel().candidateList();
        return list && list->size() > 1;
    }

    std::string onOff(bool on) {
        return on ? "Vietnamese" : "English";
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-dbus-toggle-vietnamese");
    TestInstance            testInstance;
    fcitx::NgoSenEngine     engine(&testInstance.instance);
    fcitx::NgoSenDBusObject object(&engine);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    Keys                    keys{engine, entry};

    auto                    gedit = std::make_unique<TestInputContext>(&testInstance.instance, "gedit");
    auto                    kate  = std::make_unique<TestInputContext>(&testInstance.instance, "kate");
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "a new install starts in Vietnamese", modeName(realMode.load()));

    bool on = object.toggleVietnamese();
    check(!on && realMode.load() == ngosen::Mode::Off, "the shortcut switches the focused app to English", onOff(on) + ", " + modeName(realMode.load()));
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Off, "another app follows the shortcut", modeName(realMode.load()));

    // A shortcut sharing the toggle key's modifiers, such as Ctrl+Shift+V: their release is not a second switch.
    const fcitx::KeyStates ctrl{fcitx::KeyState::Ctrl};
    const fcitx::KeyStates both{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift};
    keys.send(*kate, fcitx::Key(FcitxKey_Control_L), false);
    keys.send(*kate, fcitx::Key(FcitxKey_Shift_L, ctrl), false);
    on = object.toggleVietnamese();
    keys.send(*kate, fcitx::Key(FcitxKey_Shift_L, both), true);
    keys.send(*kate, fcitx::Key(FcitxKey_Control_L, ctrl), true);
    check(on && realMode.load() == ngosen::Mode::Sen, "the shortcut's modifiers coming up do not switch back", onOff(on) + ", " + modeName(realMode.load()));

    // With no field focused, as on the desktop, the shortcut switches what apps start in.
    keys.leave(*kate);
    on = object.toggleVietnamese();
    check(!on, "with no field the shortcut switches to English", onOff(on));
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Off, "an app focused later is in English", modeName(realMode.load()));
    keys.leave(*gedit);
    on = object.toggleVietnamese();
    check(on, "with no field the shortcut switches back", onOff(on));
    keys.focus(*gedit);
    check(realMode.load() == ngosen::Mode::Sen, "an app focused later is in Vietnamese", modeName(realMode.load()));

    // A field gone without a focus out leaves no focused field behind.
    auto popup = std::make_unique<TestInputContext>(&testInstance.instance, "popup");
    keys.focus(*popup);
    popup.reset();
    keys.focused     = nullptr;
    int  trayUpdates = 0;
    auto watcher     = testInstance.instance.watchEvent(fcitx::EventType::InputContextUpdateUI, fcitx::EventWatcherPhase::PreInputMethod, [&trayUpdates](fcitx::Event& event) {
        if (static_cast<fcitx::InputContextUpdateUIEvent&>(event).component() == fcitx::UserInterfaceComponent::StatusArea)
            ++trayUpdates;
    });
    on               = object.toggleVietnamese();
    watcher.reset();
    check(!on, "the shortcut works after the focused field is destroyed", onOff(on));
    // The tray draws from a field; the one it last drew from is gone, but other fields are open.
    check(trayUpdates > 0, "the tray icon follows the shortcut with no recent field", std::to_string(trayUpdates) + " updates");
    on = object.toggleVietnamese();

    // An app the menu keeps in English opens the menu, as the toggle key does there.
    keys.focus(*gedit);
    keys.tap(*gedit, "grave");
    keys.tap(*gedit, "e");
    on = object.toggleVietnamese();
    check(!on && menuOpen(*gedit), "the shortcut opens the menu in an app set to OFF", onOff(on));
    keys.tap(*gedit, "Escape");
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Sen, "the other apps stay in Vietnamese", modeName(realMode.load()));

    // Each app keeping its switch: the shortcut switches the focused app only.
    fcitx::RawConfig eachApp;
    eachApp.setValueByPath("ToggleScope", "Each app");
    engine.setConfig(eachApp);
    on = object.toggleVietnamese();
    check(!on, "with each app, the shortcut switches kate", onOff(on));
    auto mail = std::make_unique<TestInputContext>(&testInstance.instance, "mail");
    keys.focus(*mail);
    keys.focus(*kate);
    check(realMode.load() == ngosen::Mode::Off, "kate keeps English", modeName(realMode.load()));

    if (failures > 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "dbus_toggle_vietnamese: all checks passed\n";
    return 0;
}
