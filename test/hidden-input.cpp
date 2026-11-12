// SPDX-License-Identifier: GPL-3.0-or-later
//
// While a terminal hides input, as at a password prompt, its keys reach it as typed and stay out of the
// typing log; the toggle key still works. A pty of this test with echo turned off plays the prompt.
#include "ngosen-engine.h"
#include "ngosen-pty.h"
#include "ngosen-utils.h"
#include "test-input-context.h"

#include <fcitx/candidatelist.h>

#include <pty.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what, const std::string& actual = {}) {
        if (!ok) {
            std::cerr << "FAIL: " << what << "\n  got: " << actual << '\n';
            ++failures;
        }
    }

    struct Keys {
        fcitx::NgoSenEngine&           engine;
        const fcitx::InputMethodEntry& entry;

        // Returns whether the engine kept the key from the app.
        bool send(TestInputContext& context, fcitx::Key key, bool release) {
            fcitx::KeyEvent event(&context, key, release);
            engine.keyEvent(entry, event);
            return event.filtered();
        }

        // Returns whether the engine kept the press or the release.
        bool tap(TestInputContext& context, const char* key) {
            const bool down = send(context, fcitx::Key(key), false);
            const bool up   = send(context, fcitx::Key(key), true);
            return down || up;
        }

        bool typed(TestInputContext& context, const std::string& text) {
            bool kept = false;
            for (const char c : text)
                kept = tap(context, std::string(1, c).c_str()) || kept;
            return kept;
        }

        void ctrlShift(TestInputContext& context) {
            const fcitx::KeyStates ctrl{fcitx::KeyState::Ctrl};
            const fcitx::KeyStates both{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift};
            send(context, fcitx::Key(FcitxKey_Control_L), false);
            send(context, fcitx::Key(FcitxKey_Shift_L, ctrl), false);
            send(context, fcitx::Key(FcitxKey_Shift_L, both), true);
            send(context, fcitx::Key(FcitxKey_Control_L, ctrl), true);
        }

        void focus(TestInputContext& context) {
            context.focusIn();
            fcitx::InputContextEvent event(&context, fcitx::EventType::InputContextFocusIn);
            engine.activate(entry, event);
        }
    };

    struct Prompt {
        int  slave = -1;

        void hideInput(bool hide) const {
            termios mode{};
            tcgetattr(slave, &mode);
            if (hide)
                mode.c_lflag &= ~ECHO;
            else
                mode.c_lflag |= ECHO;
            tcsetattr(slave, TCSANOW, &mode);
        }
    };

    bool menuOpen(TestInputContext& context) {
        auto list = context.inputPanel().candidateList();
        return list && list->size() > 1;
    }

    bool loggedKeys(fcitx::NgoSenEngine& engine) {
        return engine.recorder().dump().find("down 0x") != std::string::npos;
    }

} // namespace

int main() {
    // The scan covers every pty of the user, so a real prompt open elsewhere would decide the checks.
    if (ngosen::anyPtyHidesInput()) {
        std::cout << "hidden_input: skipped, another terminal of this user hides input\n";
        return 77;
    }
    int  master = -1;
    int  slave  = -1;
    char name[64]{};
    if (openpty(&master, &slave, name, nullptr, nullptr) != 0) {
        std::cerr << "FAIL: openpty\n";
        return 1;
    }
    const Prompt prompt{slave};

    configureTestPaths("fcitx5-ngosen-hidden-input");
    TestInstance            testInstance;
    fcitx::NgoSenEngine     engine(&testInstance.instance);
    fcitx::InputMethodEntry entry("ngosen", "Ngó Sen", "vi", "ngosen");
    Keys                    keys{engine, entry};
    auto                    konsole = std::make_unique<TestInputContext>(&testInstance.instance, "org.kde.konsole", "wayland");
    auto                    edge    = std::make_unique<TestInputContext>(&testInstance.instance, "microsoft-edge", "wayland");

    keys.focus(*konsole);
    keys.ctrlShift(*konsole);
    check(realMode.load() == ngosen::Mode::Sen, "Ctrl+Shift switches to Vietnamese");
    engine.recorder().clear();
    keys.typed(*konsole, "ab ");
    check(loggedKeys(engine), "keys are logged while the terminal shows them", engine.recorder().dump());

    prompt.hideInput(true);
    engine.recorder().clear();
    const size_t commits = konsole->commits().size();
    check(!keys.typed(*konsole, "test"), "letters at the prompt reach the terminal");
    check(!keys.tap(*konsole, "grave"), "` at the prompt reaches the terminal");
    check(!menuOpen(*konsole), "` opens no menu at the prompt");
    check(konsole->commits().size() == commits, "nothing is committed at the prompt", std::to_string(konsole->commits().size() - commits));
    check(!loggedKeys(engine), "keys at the prompt stay out of the typing log", engine.recorder().dump());

    keys.ctrlShift(*konsole);
    check(realMode.load() == ngosen::Mode::Off, "Ctrl+Shift still switches at the prompt");
    keys.ctrlShift(*konsole);
    check(realMode.load() == ngosen::Mode::Sen, "Ctrl+Shift switches back at the prompt");
    // Pressed while the terminal still showed input and released at the prompt.
    prompt.hideInput(false);
    const fcitx::KeyStates ctrl{fcitx::KeyState::Ctrl};
    const fcitx::KeyStates both{fcitx::KeyState::Ctrl, fcitx::KeyState::Shift};
    keys.send(*konsole, fcitx::Key(FcitxKey_Control_L), false);
    keys.send(*konsole, fcitx::Key(FcitxKey_Shift_L, ctrl), false);
    prompt.hideInput(true);
    keys.send(*konsole, fcitx::Key(FcitxKey_Shift_L, both), true);
    keys.send(*konsole, fcitx::Key(FcitxKey_Control_L, ctrl), true);
    check(realMode.load() == ngosen::Mode::Off, "a switch that ends at the prompt still switches");
    keys.ctrlShift(*konsole);

    // The menu was opened before the prompt came up.
    prompt.hideInput(false);
    keys.tap(*konsole, "grave");
    check(menuOpen(*konsole), "` opens the menu before the prompt");
    prompt.hideInput(true);
    check(!keys.tap(*konsole, "a"), "a key at the prompt reaches the terminal past the menu");
    check(!menuOpen(*konsole), "the prompt closes the menu");

    // Only terminal windows pass keys through; a browser beside the prompt still types Vietnamese.
    keys.focus(*edge);
    engine.recorder().clear();
    keys.typed(*edge, "ab ");
    check(loggedKeys(engine), "a browser beside the prompt still takes keys", engine.recorder().dump());

    prompt.hideInput(false);
    keys.focus(*konsole);
    engine.recorder().clear();
    keys.typed(*konsole, "ab ");
    check(loggedKeys(engine), "keys are logged again once the prompt is over", engine.recorder().dump());

    close(slave);
    close(master);
    return failures == 0 ? 0 : 1;
}
