// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file messenger-defaults.cpp
 * @brief Headless test: a fresh install enables the Messenger fixes without editing ngosen.conf.
 */

#include "ngosen-engine.h"
#include "test-input-context.h"

#include <functional>
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

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-messenger-defaults");

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    const auto&         config = engine.config();

    check("WaitSurroundingEvent defaults to on", config.waitSurroundingEvent.value());
    check("MessengerSelectOvertype defaults to on", config.messengerSelectOvertype.value());
    check("WaitSurroundingSettleMs defaults to 40", config.waitSurroundingSettleMs.value() == 40);
    check("WaitSurroundingSettleFirstWordMs defaults to 60", config.waitSurroundingSettleFirstWordMs.value() == 60);

    // The config window lists what the description holds; the timing values must not show there.
    fcitx::RawConfig description;
    config.dumpDescription(description);
    std::string                                  names;
    std::function<void(const fcitx::RawConfig&)> collect = [&](const fcitx::RawConfig& node) {
        for (const auto& name : node.subItems()) {
            names += name + '\n';
            collect(*node.get(name));
        }
    };
    collect(description);
    check("SpellCheck shows in the config window", names.find("SpellCheck\n") != std::string::npos);
    check("WaitSurroundingEvent is hidden from the config window", names.find("WaitSurroundingEvent\n") == std::string::npos);

    return failures == 0 ? 0 : 1;
}
