// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ctx-rule-cleanup.cpp
 * @brief Headless regression test: a mode rule keyed by an unnamed window's
 *        context address must be dropped when that context is destroyed.
 *
 * A window that reports no program name gets a "ctx_<address>" rule when the
 * user cycles its mode. If the rule outlives the window, the rules grow for the
 * whole session and a later window allocated at the same address inherits a
 * mode the user never chose for it.
 *
 * GNOME Shell before fcitx5 5.1.22 reports every app as "gnome-shell", so a
 * saved rule for that name would override the default mode in every app.
 */

#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

    int countCtxRules(const fcitx::NgoSenEngine& engine) {
        fcitx::RawConfig raw;
        engine.getSubConfig("app_rules")->save(raw);
        int  count = 0;
        auto rules = raw.get("Rules");
        if (!rules)
            return 0;
        rules->visitSubItems([&count](fcitx::RawConfig& rule, const std::string&) {
            auto app = rule.get("App");
            if (app && isStartsWith(app->value(), "ctx_"))
                ++count;
            return true;
        });
        return count;
    }

    bool hasRule(const fcitx::NgoSenEngine& engine, const std::string& name) {
        fcitx::RawConfig raw;
        engine.getSubConfig("app_rules")->save(raw);
        bool found = false;
        if (auto rules = raw.get("Rules")) {
            rules->visitSubItems([&](fcitx::RawConfig& rule, const std::string&) {
                auto app = rule.get("App");
                found    = found || (app && app->value() == name);
                return true;
            });
        }
        return found;
    }

} // namespace

namespace {

    int cycleAndClose(const char* testName, const std::string& program) {
        configureTestPaths(testName);
        // A rule saved under this name before it became per-session must not apply.
        if (!program.empty()) {
            std::ofstream file(std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf/ngosen-app-rules.conf", std::ios::trunc);
            file << program << "=2\n";
        }
        TestInstance        testInstance;
        fcitx::NgoSenEngine engine(&testInstance.instance);
        if (!program.empty() && hasRule(engine, program)) {
            std::cerr << "[" << program << "] Expected the saved " << program << " rule to be dropped on load\n";
            return 1;
        }

        fcitx::RawConfig config;
        config.setValueByPath("Mode", "Preedit");
        config.setValueByPath("VietnameseOn", "True");
        config.setValueByPath("InputMethod", "Telex");
        config.setValueByPath("CycleModeKey/0", "F12");
        engine.setConfig(config);

        auto context = std::make_unique<TestInputContext>(&testInstance.instance, program);
        context->focusIn();
        fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, focus);

        fcitx::KeyEvent cycle(context.get(), fcitx::Key(FcitxKey_F12), false);
        engine.keyEvent(entry, cycle);

        if (countCtxRules(engine) != 1) {
            std::cerr << "[" << program << "] Expected one ctx_ rule after cycling the mode, got " << countCtxRules(engine) << '\n';
            return 1;
        }
        if (!program.empty() && hasRule(engine, program)) {
            std::cerr << "[" << program << "] Expected no saved " << program << " rule after cycling the mode\n";
            return 1;
        }

        context.reset();

        if (countCtxRules(engine) != 0) {
            std::cerr << "[" << program << "] Expected no ctx_ rule after the window closed, got " << countCtxRules(engine) << '\n';
            return 1;
        }
        return 0;
    }

    // The rule follows the app GNOME Shell reports, not the one field all its apps share.
    int shellAppRules(const char* testName) {
        configureTestPaths(testName);
        TestInstance        testInstance;
        fcitx::NgoSenEngine engine(&testInstance.instance);

        fcitx::RawConfig    config;
        config.setValueByPath("Mode", "Preedit");
        config.setValueByPath("VietnameseOn", "True");
        config.setValueByPath("InputMethod", "Telex");
        config.setValueByPath("CycleModeKey/0", "F12");
        engine.setConfig(config);

        auto context = std::make_unique<TestInputContext>(&testInstance.instance, "gnome-shell", "ibus");
        context->focusIn();
        fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, focus);

        int  failures = 0;
        auto check    = [&failures](bool ok, const char* what) {
            if (!ok) {
                std::cerr << "[shell app] " << what << '\n';
                ++failures;
            }
        };

        // Before the extension speaks the rule belongs to the context, and goes with it.
        fcitx::KeyEvent beforeName(context.get(), fcitx::Key(FcitxKey_F12), false);
        engine.keyEvent(entry, beforeName);
        check(countCtxRules(engine) == 1, "Expected one ctx_ rule before any app is reported");

        engine.shellFocusedAppChanged("kitty.desktop");
        check(engine.getProgramName(context.get()) == "kitty.desktop", "Expected the reported app's name");
        const auto      kittyDefault = realMode.load();
        fcitx::KeyEvent cycle(context.get(), fcitx::Key(FcitxKey_F12), false);
        engine.keyEvent(entry, cycle);
        const auto kittyMode = realMode.load();
        check(kittyMode != kittyDefault, "Expected the key to change kitty's mode");
        check(hasRule(engine, "kitty.desktop"), "Expected a rule saved for kitty");
        check(!hasRule(engine, "gnome-shell"), "Expected no rule for gnome-shell");

        engine.shellFocusedAppChanged("firefox.desktop");
        check(realMode.load() == kittyDefault, "Expected another app to keep its own mode");
        engine.shellFocusedAppChanged("kitty.desktop");
        check(realMode.load() == kittyMode, "Expected kitty's mode back when it gets focus");

        // The signal can beat the focus-out, and the word belongs to the app being left.
        engine.shellFocusedAppChanged("firefox.desktop");
        for (const auto sym : {FcitxKey_v, FcitxKey_a, FcitxKey_n}) {
            fcitx::KeyEvent key(context.get(), fcitx::Key(sym), false);
            engine.keyEvent(entry, key);
        }
        engine.shellFocusedAppChanged("kitty.desktop");
        check(!context->commits().empty() && context->commits().back() == "van", "Expected the word in preedit to be committed when the app changes");

        // Another input method owns the field, so its panel and menu stay untouched.
        fcitx::InputContextEvent left(context.get(), fcitx::EventType::InputContextFocusOut);
        engine.deactivate(entry, left);
        engine.shellFocusedAppChanged("firefox.desktop");
        check(realMode.load() == kittyMode, "Expected no mode change while Ngó Sen is not active");
        engine.shellFocusedAppChanged("kitty.desktop");
        engine.activate(entry, focus);

        engine.shellFocusedAppChanged("");
        check(engine.getProgramName(context.get()).rfind("ctx_", 0) == 0, "Expected a ctx_ name once no app is reported");
        engine.shellFocusedAppChanged("kitty.desktop");
        context.reset();
        check(countCtxRules(engine) == 0, "Expected the ctx_ rule gone with its context while an app is reported");
        return failures;
    }

} // namespace

int main() {
    return cycleAndClose("fcitx5-ngosen-ctx-rule-cleanup", "") + cycleAndClose("fcitx5-ngosen-ctx-rule-cleanup", "gnome-shell") +
        shellAppRules("fcitx5-ngosen-ctx-rule-cleanup-shell");
}
