// SPDX-License-Identifier: GPL-3.0-or-later
//
// FcitxHost hands the typing logic plain copies of what fcitx5 knows about the field, and the app
// checks read only those copies. Each capability flag must land in its own Field member.
#include "ngosen-app-quirks.h"
#include "ngosen-fcitx-host.h"
#include "test-input-context.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    std::string describe(const ngosen::Field& f) {
        return std::string(f.surroundingText ? "S" : "-") + (f.preedit ? "P" : "-") + (f.formattedPreedit ? "F" : "-") + (f.url ? "U" : "-") + (f.keyEventOrderFix ? "K" : "-");
    }

    ngosen::Field field(std::string frontend, std::string program) {
        ngosen::Field f;
        f.frontend = std::move(frontend);
        f.program  = std::move(program);
        return f;
    }

    void checkFlags(TestInstance& testInstance) {
        const std::vector<std::pair<fcitx::CapabilityFlag, std::string>> cases = {
            {fcitx::CapabilityFlag::SurroundingText, "S----"},  {fcitx::CapabilityFlag::Preedit, "-P---"},
            {fcitx::CapabilityFlag::FormattedPreedit, "--F--"}, {fcitx::CapabilityFlag::Url, "---U-"},
            {fcitx::CapabilityFlag::KeyEventOrderFix, "----K"},
        };
        for (const auto& [flag, expected] : cases) {
            TestInputContext context(&testInstance.instance, "gedit", "dbus");
            context.setCapabilityFlags(fcitx::CapabilityFlags{flag});
            const auto f = ngosen::FcitxHost(&context).field();
            check("flag " + expected + " maps alone, got " + describe(f), describe(f) == expected);
            check("frontend and program are copied", f.frontend == "dbus" && f.program == "gedit");
        }
    }

    void checkSurrounding(TestInstance& testInstance) {
        TestInputContext  context(&testInstance.instance);
        ngosen::FcitxHost host(&context);
        check("no report yet reads as invalid", !host.surrounding().isValid());
        context.surroundingText().setText("chào bạn", 2, 5);
        const auto s = host.surrounding();
        check("text, cursor and anchor are copied", s.isValid() && s.text() == "chào bạn" && s.cursor() == 2 && s.anchor() == 5);
        context.surroundingText().setText("mới", 3, 3);
        check("the copy keeps the old state", s.text() == "chào bạn");
        check("a new call reads the new state", host.surrounding().text() == "mới");
        context.surroundingText().invalidate();
        check("an invalidated report reads as invalid", !host.surrounding().isValid());
    }

    void checkFocus(TestInstance& testInstance) {
        TestInputContext  context(&testInstance.instance);
        ngosen::FcitxHost host(&context);
        context.focusIn();
        check("focus in is seen", host.hasFocus());
        context.focusOut();
        check("focus out is seen", !host.hasFocus());
    }

    void checkQuirks() {
        auto gtk             = field("dbus", "gedit");
        gtk.keyEventOrderFix = true;
        check("fcitx5-gtk forwards backspaces", ngosen::forwardsBackspaces(gtk));
        check("D-Bus SDL does not forward backspaces", !ngosen::forwardsBackspaces(field("dbus", "game")));
        auto snap             = field("fcitx4", "app");
        snap.formattedPreedit = true;
        check("fcitx4 GTK module forwards backspaces", ngosen::forwardsBackspaces(snap));
        check("IBus SDL does not forward backspaces", !ngosen::forwardsBackspaces(field("ibus", "SDL_App")));
        auto gtk4            = field("dbus", "nautilus");
        gtk4.surroundingText = true;
        check("fcitx5-gtk4 ignores forwarded keys", ngosen::ignoresForwardedKeys(gtk4));
        check("IBus GTK4 ignores forwarded keys", ngosen::ignoresForwardedKeys(field("ibus", "gtk4-im:nautilus")));
        auto chrome             = field("dbus", "chromium.desktop");
        chrome.keyEventOrderFix = true;
        check("Chromium selects over autocompletion", ngosen::selectsOverAutocompletion(chrome));
        auto terminal             = field("dbus", "gnome-terminal-server");
        terminal.keyEventOrderFix = true;
        check("a terminal does not select over autocompletion", !ngosen::selectsOverAutocompletion(terminal));
        check("IBus surrounding text lags", ngosen::surroundingTextLags(field("ibus", "x")));
        check("LibreOffice applies backspaces late", ngosen::appliesBackspacesLate(field("wayland", "soffice.desktop")));
        check("Firefox hides the address bar flag", ngosen::hidesAddressBarFlag(field("wayland", "firefox")));
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-host-field-mapping");
    TestInstance testInstance;
    checkFlags(testInstance);
    checkSurrounding(testInstance);
    checkFocus(testInstance);
    checkQuirks();
    return failures == 0 ? 0 : 1;
}
