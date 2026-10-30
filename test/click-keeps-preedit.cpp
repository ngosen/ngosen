// SPDX-License-Identifier: GPL-3.0-or-later
//
// A click the app does not follow with a reset, as in XIM clients such as Alacritty, leaves the
// underlined word on screen; it is committed before the next word starts.
#include "ngosen-engine.h"
#include "ngosen-globals.h"
#include "test-input-context.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

    void type(fcitx::NgoSenEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, const std::string& text) {
        for (const char c : text) {
            const fcitx::Key key(c == ' ' ? FcitxKey_space : static_cast<fcitx::KeySym>(c));
            fcitx::KeyEvent  down(&context, key, false);
            engine.keyEvent(entry, down);
            if (!down.accepted())
                context.commitString(std::string(1, c));
            fcitx::KeyEvent up(&context, key, true);
            engine.keyEvent(entry, up);
        }
    }

    std::string joined(const std::vector<std::string>& parts) {
        std::string all;
        for (const auto& part : parts)
            all += part;
        return all;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-click-keeps-preedit");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("VietnameseOn", "True");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);
    auto context = std::make_unique<TestInputContext>(&testInstance.instance, "alacritty");
    context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);

    type(engine, entry, *context, "xin vie");
    g_mouse_clicked.store(true, std::memory_order_release);
    type(engine, entry, *context, "e ");
    const std::string text = joined(context->commits());
    if (text != "xin viee ") {
        std::cerr << "FAIL: a click keeps the underlined word\n  got: '" << text << "'\n";
        return 1;
    }
    return 0;
}
