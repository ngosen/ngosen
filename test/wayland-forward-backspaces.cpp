// SPDX-License-Identifier: GPL-3.0-or-later
//
// On the Wayland frontends ("wayland" is input-method-v1 as in KWin, "wayland_v2" is v2 as in Sway and
// Hyprland) the backspaces of a replacement go through forwardKey instead of real key presses, and the
// commit still waits until the app reports the deletion done.
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "key-sender-probe.h"
#include "test-input-context.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

    void setSnapshot(TestInputContext& context, const std::string& text, unsigned int cursor) {
        context.surroundingText().setText(text, cursor, cursor);
        context.updateSurroundingText();
    }

    bool type(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInputContext& context, fcitx::KeySym symbol, bool accepted) {
        fcitx::KeyEvent event(&context, fcitx::Key(symbol), false);
        engine.keyEvent(entry, event);
        if (event.accepted() != accepted) {
            reportFailure("process key " + std::to_string(symbol), "accepted=" + std::to_string(accepted), "accepted=" + std::to_string(event.accepted()));
            return false;
        }
        return true;
    }

    std::string describeForwarded(const TestInputContext& context) {
        std::string out;
        for (const auto& event : context.forwarded())
            out += "[" + event.key().toString() + (event.isRelease() ? " up" : " down") + "]";
        return out.empty() ? "(none)" : out;
    }

    bool replacesThroughForward(fcitx::LotusEngine& engine, const fcitx::InputMethodEntry& entry, TestInstance& testInstance, const std::string& frontend) {
        auto context = std::make_unique<TestInputContext>(&testInstance.instance, "test", frontend);
        context->setCapabilityFlags(fcitx::CapabilityFlag::SurroundingText);
        context->focusIn();
        fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, focus);

        setSnapshot(*context, "\n\n", 0);
        const std::string word = "tie";
        for (size_t i = 0; i < word.size(); ++i) {
            if (!type(engine, entry, *context, static_cast<fcitx::KeySym>(word[i]), false))
                return false;
            setSnapshot(*context, word.substr(0, i + 1) + "\n\n", static_cast<unsigned int>(i + 1));
        }

        // Telex "e" again: e -> ê deletes one character.
        if (!type(engine, entry, *context, FcitxKey_e, true))
            return false;
        const auto& forwarded = context->forwarded();
        if (forwarded.size() != 2 || forwarded[0].key().sym() != FcitxKey_BackSpace || forwarded[0].isRelease() || forwarded[1].key().sym() != FcitxKey_BackSpace ||
            !forwarded[1].isRelease()) {
            reportFailure(frontend + ": forward the backspace for e -> ê", "[BackSpace down][BackSpace up]", describeForwarded(*context));
            return false;
        }

        // The app has not reported the deletion yet, so nothing may be committed.
        pumpEventLoop(testInstance.instance, 3);
        if (!context->commits().empty()) {
            reportFailure(frontend + ": no commit before the app reports the deletion", "commits=(none)", "commits=" + joinCommits(*context));
            return false;
        }

        setSnapshot(*context, "ti\n\n", 2);
        pumpEventLoop(testInstance.instance, 120);
        if (context->commits() != std::vector<std::string>{"ê"}) {
            reportFailure(frontend + ": commit once the app reports the deletion", "commits=['ê']", "commits=" + joinCommits(*context));
            return false;
        }
        return true;
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-wayland-forward-backspaces");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    config.setValueByPath("WaitSurroundingEvent", "True");
    // Left at its default on purpose: the "\n\n" snapshot below looks like a Facebook composer, whose
    // Shift+Left overtype needs real key presses and must not run on these frontends.
    config.setValueByPath("MessengerSelectOvertype", "True");
    engine.setConfig(config);

    KeySenderProbe          keys;
    fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
    for (const char* frontend : {"wayland", "wayland_v2"}) {
        if (!replacesThroughForward(engine, entry, testInstance, frontend))
            return 1;
    }

    if (const int requests = keys.requests(); requests != 0) {
        reportFailure("no real key press", "0", std::to_string(requests));
        return 1;
    }
    return 0;
}
