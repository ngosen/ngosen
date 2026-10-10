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
 */

#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

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

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-ctx-rule-cleanup");

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);

    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("CycleModeKey/0", "F12");
    engine.setConfig(config);

    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "");
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    fcitx::KeyEvent cycle(context.get(), fcitx::Key(FcitxKey_F12), false);
    engine.keyEvent(entry, cycle);

    if (countCtxRules(engine) != 1) {
        std::cerr << "Expected one ctx_ rule after cycling the mode, got " << countCtxRules(engine) << '\n';
        return 1;
    }

    context.reset();

    if (countCtxRules(engine) != 0) {
        std::cerr << "Expected no ctx_ rule after the window closed, got " << countCtxRules(engine) << '\n';
        return 1;
    }
    return 0;
}
