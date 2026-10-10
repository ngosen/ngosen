// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>
#include <iostream>

int main() {
    configureTestPaths("fcitx5-ngosen-preedit-client");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::NgoSenMode::Preedit || engine.config().inputMethod.value() != "Telex") {
        std::cerr << "failed to configure Preedit/Telex\n";
        return 1;
    }
    auto context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);
    context->resetPreeditUpdateCount();

    // Bamboo Telex processes a then s as U+00E1 LATIN SMALL LETTER A WITH ACUTE.
    for (auto sym : {FcitxKey_a, FcitxKey_s}) {
        fcitx::KeyEvent event(context.get(), fcitx::Key(sym), false);
        engine.keyEvent(entry, event);
        if (!event.accepted()) {
            std::cerr << "key " << sym << " accepted=" << event.accepted() << '\n';
            return 1;
        }
    }
    const auto& preedit = context->inputPanel().clientPreedit();
    if (preedit.toString() != "á" || !fcitx::utf8::validate(preedit.toString()) || preedit.cursor() != 2 || context->preeditUpdates() != 2 || !context->commits().empty() ||
        !context->inputPanel().preedit().toString().empty()) {
        std::cerr << "client='" << preedit.toString() << "' server='" << context->inputPanel().preedit().toString() << "' cursor=" << preedit.cursor()
                  << " updates=" << context->preeditUpdates() << " commits=" << context->commits().size() << '\n';
        return 1;
    }
    return 0;
}
