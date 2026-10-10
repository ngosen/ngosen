// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file messenger-defaults.cpp
 * @brief Headless test: a fresh install enables the Messenger fixes without editing ngosen.conf.
 */

#include "lotus-engine.h"
#include "test-input-context.h"

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
    configureTestPaths("fcitx5-lotus-messenger-defaults");

    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    const auto&        config = engine.config();

    check("WaitSurroundingEvent defaults to on", config.waitSurroundingEvent.value());
    check("MessengerSelectOvertype defaults to on", config.messengerSelectOvertype.value());
    check("WaitSurroundingSettleMs defaults to 40", config.waitSurroundingSettleMs.value() == 40);
    check("WaitSurroundingSettleFirstWordMs defaults to 60", config.waitSurroundingSettleFirstWordMs.value() == 60);

    return failures == 0 ? 0 : 1;
}
