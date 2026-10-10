// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-engine.h"
#include "ngosen-utils.h"
#include "key-sender-probe.h"
#include "test-input-context.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual, const std::string& meaning) {
        std::cerr << "Step: " << step << '\n';
        std::cerr << "Expected: " << expected << '\n';
        std::cerr << "Actual: " << actual << '\n';
        std::cerr << "Meaning: " << meaning << '\n';
    }

    // The window counts characters, not commits: "dd" is two of them.
    std::vector<std::string> splitUtf8(const std::string& text) {
        std::vector<std::string> characters;
        for (size_t i = 0; i < text.size();) {
            const auto lead   = static_cast<unsigned char>(text[i]);
            size_t     length = 1;
            if ((lead & 0xF8U) == 0xF0U)
                length = 4;
            else if ((lead & 0xF0U) == 0xE0U)
                length = 3;
            else if ((lead & 0xE0U) == 0xC0U)
                length = 2;
            length = std::min(length, text.size() - i);
            characters.push_back(text.substr(i, length));
            i += length;
        }
        return characters;
    }

} // namespace

int main(int argc, char** argv) {
    // "paced" is the control: same presses, but none of them arrive while a
    // replacement is still in flight, so nothing lands in buffered_keys_.
    const bool paced = argc > 1 && std::string(argv[1]) == "paced";

    // Own a private socket name so a running fcitx5-lotus-server does not
    // already hold the one the listener binds.

    configureTestPaths("fcitx5-ngosen-held-key-repeat");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Sen");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    KeySenderProbe listener;
    auto           context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->focusIn();
    fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);
    context->resetPreeditUpdateCount();

    // Reproduces #472: a held 'd' under Telex.
    //
    // The window model matters. In Uinput mode the uinput server passes the
    // physical keystroke straight to the application, so every press shows up
    // there on its own; the engine only corrects afterwards with backspaces and a
    // commit. Counting commits alone would understate what the user sees.
    const int                kHeldPresses = 12;
    int                      sent         = 0;
    std::vector<std::string> screen;
    size_t                   lastCommit = 0;

    auto                     press = [&]() {
        screen.emplace_back("d"); // the application sees the raw key first
        fcitx::KeyEvent event(context.get(), fcitx::Key(FcitxKey_d), false);
        engine.keyEvent(entry, event);
        ++sent;
        return event.accepted();
    };

    auto pullCommit = [&]() {
        if (context->commits().size() > lastCommit) {
            for (auto& character : splitUtf8(context->commits().back()))
                screen.push_back(character);
            lastCommit = context->commits().size();
        }
    };

    while (sent < kHeldPresses && !press()) {}

    int cycles = 0;
    while (cycles < 8) {
        int backspaces = 0;
        if (!listener.receive(backspaces, "no further replacement request"))
            break;
        ++cycles;
        for (int i = 0; i < backspaces && !screen.empty(); ++i)
            screen.pop_back();

        if (!paced) {
            for (int i = 0; i < 2 && sent < kHeldPresses; ++i)
                press();
        }

        for (int i = 0; i < backspaces; ++i) {
            fcitx::KeyEvent back(context.get(), fcitx::Key(FcitxKey_BackSpace), false);
            engine.keyEvent(entry, back);
        }
        pumpEventLoop(testInstance.instance, 50); // the commit runs from a timer
        pullCommit();

        if (paced) {
            while (sent < kHeldPresses && !press()) {}
        }
    }
    pullCommit();

    std::string text;
    for (const auto& piece : screen)
        text += piece;
    std::cerr << (paced ? "[paced control] " : "[held key] ") << "keys sent: " << sent << ", replacement cycles: " << cycles << "\ncommits: ";
    for (const auto& commit : context->commits())
        std::cerr << "['" << commit << "']";
    std::cerr << "\nforwarded: " << context->forwarded().size() << "\nwindow shows: '" << text << "' (" << screen.size() << " characters)\n";

    // Outside Uinput mode a held key keeps appending, so the window grows with
    // the number of presses. #472 reports that it stops instead.
    if (screen.size() < 3) {
        reportFailure("held key accumulates", "at least 3 characters after " + std::to_string(sent) + " presses", "window shows '" + text + "'",
                      "a held key never grows past the Telex cycle: handleUinputMode() resets the Bamboo engine "
                      "after every commit, so each repeat restarts from an empty word buffer");
        return 1;
    }
    return 0;
}
