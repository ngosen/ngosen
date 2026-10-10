// SPDX-License-Identifier: GPL-3.0-or-later
//
// An XIM client sometimes sends a key we let through back to us instead of typing it: the same
// keycode with the same timestamp. It must go to the app again, not be typed a second time.
// Other frontends never do that, and WPS's fcitx4 plugin stamps keys in whole seconds, so there two
// quick presses of one key share a time and both must be typed.
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual) {
        std::cerr << "Step: " << step << "\nExpected: " << expected << "\nActual: " << actual << '\n';
    }

    std::string joinCommits(const TestInputContext& context) {
        std::string out;
        for (const auto& commit : context.commits())
            out += "['" + commit + "']";
        return out.empty() ? "(none)" : out;
    }

    // X keycodes of a US layout.
    constexpr int CodeC = 54;
    constexpr int CodeO = 32;

    struct Harness {
        fcitx::NgoSenEngine&           engine;
        const fcitx::InputMethodEntry& entry;
        TestInputContext&              context;

        bool                           press(fcitx::KeySym symbol, int code, int time, bool accepted, const std::string& step) {
            fcitx::KeyEvent event(&context, fcitx::Key(symbol, fcitx::KeyStates(), code), false, time);
            engine.keyEvent(entry, event);
            if (event.accepted() != accepted) {
                reportFailure(step, "accepted=" + std::to_string(accepted), "accepted=" + std::to_string(event.accepted()));
                return false;
            }
            return true;
        }
    };

} // namespace

int main() {
    unsetenv("WAYLAND_DISPLAY");

    configureTestPaths("fcitx5-ngosen-xim-key-handback");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "test", "xim");
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    Harness h{engine, entry, *context};
    if (!h.press(FcitxKey_c, CodeC, 1000, false, "type c") || !h.press(FcitxKey_o, CodeO, 1100, false, "type o"))
        return 1;
    if (!h.press(FcitxKey_o, CodeO, 1100, false, "the client sends the same o back"))
        return 1;
    pumpEventLoop(testInstance.instance, 50);
    if (!context->commits().empty()) {
        reportFailure("nothing typed for the handed-back o", "(none)", joinCommits(*context));
        return 1;
    }
    // A second o pressed later is a new key: co -> cô.
    if (!h.press(FcitxKey_o, CodeO, 1250, true, "type o again"))
        return 1;
    pumpEventLoop(testInstance.instance, 100);
    if (context->commits().empty() || context->commits().back() != "ô") {
        reportFailure("co + o", "last commit 'ô'", joinCommits(*context));
        return 1;
    }
    context->focusOut();

    auto wps = std::make_unique<TestInputContext>(&testInstance.instance, "wps", "fcitx4");
    // What WPS sets, though it never reports any text.
    wps->setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::ClientUnfocusCommit, fcitx::CapabilityFlag::SurroundingText});
    wps->focusIn();
    fcitx::InputContextEvent wpsFocus(wps.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, wpsFocus);
    Harness       w{engine, entry, *wps};
    constexpr int Second = 1791536707;
    if (!w.press(FcitxKey_c, CodeC, Second, false, "fcitx4: type c") || !w.press(FcitxKey_o, CodeO, Second, false, "fcitx4: type o"))
        return 1;
    if (!w.press(FcitxKey_o, CodeO, Second, true, "fcitx4: type o again in the same second"))
        return 1;
    pumpEventLoop(testInstance.instance, 100);
    if (wps->commits().empty() || wps->commits().back() != "ô") {
        reportFailure("fcitx4: co + o", "last commit 'ô'", joinCommits(*wps));
        return 1;
    }
    return 0;
}
