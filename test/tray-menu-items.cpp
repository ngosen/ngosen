// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tray menu shows the entries the user chose; Vietnamese, Settings and Save typing log always show. A
// choice entry names the current choice, since GNOME's kimpanel shows no check marks.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <fcitx/action.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    std::vector<std::string> menu(TestInputContext& context) {
        std::vector<std::string> names;
        for (auto* action : context.statusArea().actions(fcitx::StatusGroup::InputMethod))
            names.push_back(action->name());
        return names;
    }

    bool has(const std::vector<std::string>& names, const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    }

    fcitx::Action* entryNamed(TestInputContext& context, const std::string& name) {
        for (auto* action : context.statusArea().actions(fcitx::StatusGroup::InputMethod))
            if (action->name() == name)
                return action;
        return nullptr;
    }

    std::vector<std::string> choices(TestInputContext& context, fcitx::Action& entry) {
        std::vector<std::string> labels;
        for (auto* action : entry.menu()->actions())
            labels.push_back(action->shortText(&context));
        return labels;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-tray-menu-items");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);

    TestInputContext    context(&testInstance.instance);
    context.focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent in(&context, fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, in);

    // The way into Vietnamese without the toggle key: the first entry, marked like the switches.
    check("the Vietnamese entry comes first", !menu(context).empty() && menu(context).front() == "ngosen-vietnamese");
    auto* vietnamese = entryNamed(context, "ngosen-vietnamese");
    check("the Vietnamese entry starts off", vietnamese && vietnamese->shortText(&context) == "✖ Vietnamese");
    if (vietnamese) {
        vietnamese->activate(&context);
        check("a click turns Vietnamese on", ::realMode.load() != fcitx::NgoSenMode::Off);
        check("a click marks the entry", vietnamese->shortText(&context) == "✔ Vietnamese");
        // Clicked with no field focused, it switches the mode new apps start in.
        vietnamese->activate(nullptr);
        check("a click with no field turns Vietnamese off", ::realMode.load() == fcitx::NgoSenMode::Off);
        check("a click with no field marks the entry", vietnamese->shortText(&context) == "✖ Vietnamese");
        vietnamese->activate(&context);
        // Turning Vietnamese off another way unmarks the entry too.
        for (auto sym : {FcitxKey_grave, FcitxKey_e}) {
            fcitx::KeyEvent down(&context, fcitx::Key(sym), false);
            engine.keyEvent(entry, down);
            fcitx::KeyEvent up(&context, fcitx::Key(sym), true);
            engine.keyEvent(entry, up);
        }
        check("picking Off in the mode menu turns Vietnamese off", ::realMode.load() == fcitx::NgoSenMode::Off);
        check("picking Off in the mode menu unmarks the entry", vietnamese->shortText(&context) == "✖ Vietnamese");
        vietnamese->activate(&context);
    }

    auto* inputMethod = entryNamed(context, "ngosen-input-method");
    check("the entry names the input method", inputMethod && inputMethod->shortText(&context) == "Input Method (Telex)");
    check("the current input method is marked", inputMethod && choices(context, *inputMethod) == std::vector<std::string>{"✔ Telex", "VNI"});
    if (inputMethod) {
        inputMethod->menu()->actions().back()->activate(&context);
        check("picking VNI renames the entry", inputMethod->shortText(&context) == "Input Method (VNI)");
        check("picking VNI moves the mark", choices(context, *inputMethod) == std::vector<std::string>{"Telex", "✔ VNI"});
    }
    auto* charset = entryNamed(context, "ngosen-charset");
    check("the entry names the charset", charset && charset->shortText(&context) == "Charset (Unicode)");

    // A switch entry keeps the name the settings window gives it, before and after a click.
    auto*      restore = entryNamed(context, "ngosen-autonvnrestore");
    const auto mark    = ngosen::defaultOptions.autoNonVnRestore ? "✔ " : "✖ ";
    check("the restore entry uses the settings name", restore && restore->shortText(&context) == std::string(mark) + "Auto Restore Invalid Words");
    if (restore) {
        restore->activate(&context);
        restore->activate(&context);
        check("the restore entry keeps that name after a click", restore->shortText(&context) == std::string(mark) + "Auto Restore Invalid Words");
    }

    // Kimpanel sends an entry as "key:text:icon:..." and splits it on every colon.
    bool colonFree = true;
    for (auto* action : context.statusArea().actions(fcitx::StatusGroup::InputMethod)) {
        colonFree = colonFree && action->shortText(&context).find(':') == std::string::npos;
        if (action->menu())
            for (const auto& label : choices(context, *action))
                colonFree = colonFree && label.find(':') == std::string::npos;
    }
    check("no menu text has a colon", colonFree);

    auto names = menu(context);
    check("every entry shows by default", names.size() == 11 && has(names, "ngosen-input-method") && has(names, "ngosen-charset"));

    fcitx::RawConfig hidden;
    hidden.setValueByPath("ShowMenuCharset", "False");
    engine.setConfig(hidden);
    check("the focused field's menu changes at once", !has(menu(context), "ngosen-charset"));
    engine.activate(entry, in);
    names = menu(context);
    check("a hidden entry leaves the menu", !has(names, "ngosen-charset") && names.size() == 10);
    check("Settings, Save typing log and Vietnamese stay", has(names, "ngosen-settings") && has(names, "ngosen-save-log") && has(names, "ngosen-vietnamese"));

    // The tray keeps showing the last field's menu while the settings window has focus.
    context.focusOut();
    hidden.setValueByPath("ShowMenuSpellCheck", "False");
    engine.setConfig(hidden);
    check("the menu left in the tray changes at once", !has(menu(context), "ngosen-spellcheck"));
    context.focusIn();
    engine.activate(entry, in);

    const auto before = menu(context).size();
    engine.activate(entry, in);
    check("filling again does not repeat entries", menu(context).size() == before);

    return failures == 0 ? 0 : 1;
}
