// SPDX-License-Identifier: GPL-3.0-or-later
//
// With the Custom input method, typing follows the keymap the user defined, and only while the
// custom keymap is turned on.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

    int  failures = 0;

    void check(const std::string& step, const std::string& expected, const std::string& actual) {
        if (actual != expected) {
            std::cerr << "FAIL: " << step << ": expected [" << expected << "], got [" << actual << "]\n";
            ++failures;
        }
    }

    std::string typeAndRead(TestInstance& testInstance, fcitx::NgoSenEngine& engine) {
        auto context = std::make_unique<TestInputContext>(&testInstance.instance);
        context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
        context->focusIn();
        fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
        fcitx::InputContextEvent in(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, in);
        for (auto sym : {FcitxKey_a, FcitxKey_q}) {
            fcitx::KeyEvent event(context.get(), fcitx::Key(sym), false);
            engine.keyEvent(entry, event);
        }
        return context->inputPanel().clientPreedit().toString();
    }

} // namespace

int main() {
    const char* testName = "ngosen-custom-keymap";
    configureTestPaths(testName);
    // The keymap is read when the engine starts, so it must be on disk first.
    const auto keymapFile = std::filesystem::temp_directory_path() / testName / "config/fcitx5/conf/ngosen-custom-keymap.conf";
    {
        std::ofstream file(keymapFile, std::ios::trunc);
        file << "[CustomKeymap]\n[CustomKeymap/0]\nKey=q\nValue=DauSac\n";
    }

    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("InputMethod", "Custom");
    config.setValueByPath("EnableCustomKeymap", "True");
    engine.setConfig(config);
    check("q adds the acute accent from the custom keymap", "á", typeAndRead(testInstance, engine));

    config.setValueByPath("EnableCustomKeymap", "False");
    engine.setConfig(config);
    check("the keymap is ignored while turned off", "aq", typeAndRead(testInstance, engine));
    return failures == 0 ? 0 : 1;
}
