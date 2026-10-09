/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// The parts of the IBus shell that run without an IBus daemon: settings, key presses and timers.
#include "ngosen-ibus-config.h"
#include "ngosen-ibus-host.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    std::string writeTemp(const std::string& content) {
        char      path[] = "/tmp/ngosen-ibus-settings-XXXXXX";
        const int fd     = mkstemp(path);
        close(fd);
        std::ofstream(path) << content;
        return path;
    }

    void testSettings() {
        const auto defaults = ngosen::readSettings("/nonexistent/lotus.conf");
        check(defaults.options.inputMethod == "Telex", "settings: Telex without a file");
        check(defaults.options.spellCheck, "settings: spell check on without a file");
        check(defaults.mode == ngosen::Mode::Sen, "settings: Sen without a file");

        const std::string path = writeTemp("# comment\n"
                                           "Mode=Preedit\n"
                                           "InputMethod=VNI\n"
                                           "SpellCheck=False\n"
                                           "WaitSurroundingTimeoutMs=70\n"
                                           "OutputCharset=\"VNI Windows\"\n"
                                           "[AppRules]\n"
                                           "InputMethod=Telex\n");
        const auto        read = ngosen::readSettings(path);
        std::remove(path.c_str());
        check(read.mode == ngosen::Mode::Preedit, "settings: Preedit mode");
        check(read.options.inputMethod == "VNI", "settings: input method");
        check(!read.options.spellCheck, "settings: False turns spell check off");
        check(read.options.waitSurroundingTimeoutMs == 70, "settings: integer value");
        check(read.options.outputCharset == "VNI Windows", "settings: quoted value");
        check(read.options.modernStyle, "settings: missing key keeps its default");

        const std::string emoji = writeTemp("Mode=Emoji Picker\n");
        check(ngosen::readSettings(emoji).mode == ngosen::Mode::Sen, "settings: modes without IBus support fall back to Sen");
        std::remove(emoji.c_str());
    }

    void testKeyPress() {
        ngosen::IBusKeyPress release(IBUS_KEY_a, 30, IBUS_RELEASE_MASK | IBUS_CONTROL_MASK);
        check(release.isRelease(), "key: release mask means release");
        check(release.states() == IBUS_CONTROL_MASK, "key: states drop the release mask");
        check(release.hasModifier(), "key: Ctrl is a modifier");
        check(release.code() == 38, "key: X11 keycode is the evdev code plus 8");
        check(release.name() == "Control+a", "key: name with modifier");

        ngosen::IBusKeyPress shift(IBUS_KEY_Shift_L, 42, IBUS_SHIFT_MASK | IBUS_MOD2_MASK);
        check(shift.isModifier() && shift.isBareShift(), "key: Shift with Num Lock on is a bare Shift");
        ngosen::IBusKeyPress ctrlShift(IBUS_KEY_Shift_L, 42, IBUS_CONTROL_MASK);
        check(!ctrlShift.isBareShift(), "key: Shift with Ctrl held is not bare");

        ngosen::IBusKeyPress letter(IBUS_KEY_a, 30, 0);
        check(letter.replacedSym() == 0, "key: unchanged key has no replacement");
        letter.replaceSym(IBUS_KEY_A);
        check(letter.replacedSym() == IBUS_KEY_A, "key: replaced symbol is reported");
        check(!letter.accepted(), "key: replacing does not accept");
    }

    void testTimer() {
        int          fired = 0;
        auto         timer = ngosen::startGlibTimer(g_get_monotonic_time() + 5000, [&fired](ngosen::Timer& self) {
            if (++fired < 3) {
                self.rearm(g_get_monotonic_time() + 2000);
                return true;
            }
            return false;
        });
        const gint64 end   = g_get_monotonic_time() + 200000;
        while (g_get_monotonic_time() < end)
            g_main_context_iteration(nullptr, FALSE);
        check(fired == 3, "timer: fires, rearms twice, then stops (fired " + std::to_string(fired) + ")");

        bool cancelled = true;
        auto later     = ngosen::startGlibTimer(g_get_monotonic_time() + 5000, [&cancelled](ngosen::Timer&) {
            cancelled = false;
            return false;
        });
        later.reset();
        const gint64 until = g_get_monotonic_time() + 30000;
        while (g_get_monotonic_time() < until)
            g_main_context_iteration(nullptr, FALSE);
        check(cancelled, "timer: destroying it cancels it");
    }

} // namespace

int main() {
    testSettings();
    testKeyPress();
    testTimer();
    if (failures == 0)
        std::cout << "all IBus shell checks passed\n";
    return failures == 0 ? 0 : 1;
}
