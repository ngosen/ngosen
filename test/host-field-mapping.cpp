// SPDX-License-Identifier: GPL-3.0-or-later
//
// FcitxHost hands the typing logic plain copies of what fcitx5 knows about the field, and the app
// checks read only those copies. Each capability flag must land in its own Field member, and the
// preedit must reach the app or the panel as before. A key press must read and change the fcitx5
// key event the way the typing logic used to.
#include "ngosen-app-quirks.h"
#include "ngosen-fcitx-host.h"
#include "ngosen-gnome-shell.h"
#include "ngosen-program-name.h"
#include "test-input-context.h"

#include <fcitx/event.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
            const auto f = ngosen::FcitxHost(&context, &testInstance.instance).field();
            check("flag " + expected + " maps alone, got " + describe(f), describe(f) == expected);
            check("frontend and program are copied", f.frontend == "dbus" && f.program == "gedit");
        }
        TestInputContext context(&testInstance.instance, "chromium", "dbus");
        context.setCursorRect(fcitx::Rect(168, 86, 169, 103));
        const auto caret = ngosen::FcitxHost(&context, &testInstance.instance).field();
        check("the caret size is copied", caret.caretWidth == 1 && caret.caretHeight == 17);
    }

    void checkShellDropsKeys(TestInstance& testInstance) {
        for (const char* version : {"50.0", "50.1", "50.3"})
            check(std::string("GNOME ") + version + " drops forwarded keys", ngosen::gnomeShellDropsForwardedKeys(version));
        for (const char* version : {"50.4", "50.10", "49.5", "51.1", "50", "50.rc", ""})
            check(std::string("GNOME '") + version + "' keeps forwarded keys", !ngosen::gnomeShellDropsForwardedKeys(version));

        bool drops     = true;
        auto sendsKeys = [&](const char* program, const char* frontend, const bool* shellDropsKeys) {
            TestInputContext context(&testInstance.instance, program, frontend);
            return !ngosen::FcitxHost(&context, &testInstance.instance, shellDropsKeys).field().sendsNoKeys;
        };
        check("apps relayed by a dropping GNOME Shell are sent no keys", !sendsKeys("gnome-shell", "ibus", &drops));
        check("an app on its own IBus connection still gets keys", sendsKeys("chromium", "ibus", &drops));
        check("fcitx5's own GTK module still gets keys", sendsKeys("gnome-shell", "dbus", &drops));
        check("without a host flag apps get keys", sendsKeys("gnome-shell", "ibus", nullptr));
        drops = false;
        check("a GNOME Shell that keeps keys gets them", sendsKeys("gnome-shell", "ibus", &drops));
    }

    void checkShellFocusedApp(TestInstance& testInstance) {
        std::string focused = "firefox_firefox.desktop";
        auto        field   = [&](const char* program, const char* frontend, const std::string* shellFocusedApp) {
            TestInputContext context(&testInstance.instance, program, frontend);
            return ngosen::FcitxHost(&context, &testInstance.instance, nullptr, shellFocusedApp).field();
        };
        const auto relayed = field("gnome-shell", "ibus", &focused);
        check("an app GNOME Shell relays takes the focused app's name", relayed.program == "firefox_firefox.desktop");
        check("the name GNOME Shell reported is kept", relayed.reportedProgram == "gnome-shell");
        check("the focused app still gets GNOME Shell's edit limits", ngosen::keepsLastEditPerTurn(relayed));
        check("an app with its own name keeps it", field("kitty", "ibus", &focused).program == "kitty");
        check("an app with its own name has no edit limits", !ngosen::keepsLastEditPerTurn(field("kitty", "ibus", &focused)));
        check("without the extension the name stays", field("gnome-shell", "ibus", nullptr).program == "gnome-shell");
        focused.clear();
        const auto unknown = field("gnome-shell", "ibus", &focused);
        check("with no focused app the name stays", unknown.program == "gnome-shell" && unknown.reportedProgram.empty());
        check("with no focused app the edit limits stay", ngosen::keepsLastEditPerTurn(unknown));
        focused = "firefox.desktop";
        check("fcitx5's own GTK module keeps its name", field("gnome-shell", "dbus", &focused).program == "gnome-shell");
    }

    void checkSurrounding(TestInstance& testInstance) {
        TestInputContext  context(&testInstance.instance);
        ngosen::FcitxHost host(&context, &testInstance.instance);
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
        ngosen::FcitxHost host(&context, &testInstance.instance);
        context.focusIn();
        check("focus in is seen", host.hasFocus());
        context.focusOut();
        check("focus out is seen", !host.hasFocus());
    }

    void checkPreedit(TestInstance& testInstance) {
        TestInputContext context(&testInstance.instance);
        context.setCapabilityFlags(fcitx::CapabilityFlags{fcitx::CapabilityFlag::Preedit});
        ngosen::FcitxHost host(&context, &testInstance.instance);
        auto&             panel = context.inputPanel();
        host.showPreedit("tiếng", true);
        check("the app draws the preedit when it can", panel.clientPreedit().toString() == "tiếng" && panel.preedit().toString().empty());
        check("underlined preedit", panel.clientPreedit().size() == 1 && panel.clientPreedit().formatAt(0) == fcitx::TextFormatFlag::Underline);
        check("cursor at the end of the preedit", panel.clientPreedit().cursor() == static_cast<int>(std::string("tiếng").size()));
        const unsigned int updates = context.preeditUpdates();
        host.refreshPreedit();
        check("refreshing sends the preedit", context.preeditUpdates() == updates + 1);
        host.showPreedit("a", false);
        check("plain preedit", panel.clientPreedit().formatAt(0) == fcitx::TextFormatFlag::NoFlag);
        host.clearPreedit();
        check("clearing empties the app's preedit", panel.clientPreedit().toString().empty());

        TestInputContext  plain(&testInstance.instance);
        ngosen::FcitxHost plainHost(&plain, &testInstance.instance);
        plainHost.showPreedit("a", false);
        check("the panel draws the preedit otherwise", plain.inputPanel().preedit().toString() == "a" && plain.inputPanel().clientPreedit().toString().empty());
        plainHost.clearPreedit();
        check("clearing empties the panel's preedit", plain.inputPanel().preedit().toString().empty());
        plainHost.showPreedit("a", false);
        plainHost.resetPanel();
        check("resetting empties the panel", plain.inputPanel().preedit().toString().empty());
    }

    void checkKeys(TestInstance& testInstance) {
        TestInputContext  context(&testInstance.instance);
        ngosen::FcitxHost host(&context, &testInstance.instance);
        auto              press = [&](fcitx::Key key, bool release, auto&& onPress) {
            fcitx::KeyEvent       event(&context, key, release);
            ngosen::FcitxKeyPress p(event);
            onPress(p, event);
        };
        using fcitx::KeyState;
        press(fcitx::Key(FcitxKey_a, KeyState::Ctrl), true, [](ngosen::KeyPress& p, fcitx::KeyEvent&) {
            check("symbol, modifiers and release are copied", p.sym() == FcitxKey_a && p.states() == static_cast<uint32_t>(KeyState::Ctrl) && p.isRelease());
            check("Ctrl+a has a modifier", p.hasModifier());
            check("Ctrl+a is named as fcitx5 normalizes it", p.name() == "Control+A");
            check("a letter is no modifier key", !p.isModifier() && !p.isBareShift());
            check("a letter does not move the cursor", !p.isCursorMove());
        });
        press(fcitx::Key(FcitxKey_a), false, [](ngosen::KeyPress& p, fcitx::KeyEvent& e) {
            check("a has no modifier", !p.hasModifier() && !p.isRelease());
            p.replaceSym(FcitxKey_A);
            check("replacing changes the key the app gets", e.key().sym() == FcitxKey_A && p.name() == "A");
            check("replacing keeps the key as pressed", p.sym() == FcitxKey_a && e.rawKey().sym() == FcitxKey_a);
            check("a key is left for the app by default", !e.filtered() && !e.accepted());
            p.passToApp();
            check("passing to the app changes nothing", !e.filtered() && !e.accepted());
            p.accept();
            check("accepting keeps the key from the app", e.filtered() && e.accepted());
        });
        press(fcitx::Key(FcitxKey_a, KeyState::Super), false, [](ngosen::KeyPress& p, fcitx::KeyEvent& e) {
            p.replaceSym(FcitxKey_A);
            check("replacing keeps the modifiers", e.key().states() == KeyState::Super && p.hasModifier());
        });
        press(fcitx::Key(FcitxKey_Shift_L), false, [](ngosen::KeyPress& p, fcitx::KeyEvent&) { check("left Shift alone is bare", p.isBareShift() && p.isModifier()); });
        press(fcitx::Key(FcitxKey_Shift_R, KeyState::Shift), true, [](ngosen::KeyPress& p, fcitx::KeyEvent&) { check("releasing right Shift is bare", p.isBareShift()); });
        press(fcitx::Key(FcitxKey_Shift_L, KeyState::Ctrl), false, [](ngosen::KeyPress& p, fcitx::KeyEvent&) { check("Shift with Ctrl held is not bare", !p.isBareShift()); });
        press(fcitx::Key(FcitxKey_Control_L), false, [](ngosen::KeyPress& p, fcitx::KeyEvent&) { check("Ctrl is a modifier key", p.isModifier() && !p.isBareShift()); });
        press(fcitx::Key(FcitxKey_Left), false, [](ngosen::KeyPress& p, fcitx::KeyEvent&) { check("Left moves the cursor", p.isCursorMove()); });
        check("a letter types itself", host.keyText(FcitxKey_a) == "a");
        check("a Vietnamese keysym types its letter", host.keyText(0x1001ea1) == "ạ");
        check("Shift types nothing", host.keyText(FcitxKey_Shift_L).empty());
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
        check("a Chromium page field does not select over autocompletion", !ngosen::selectsOverAutocompletion(chrome));
        chrome.caretWidth  = 239;
        chrome.caretHeight = 15;
        check("a selection in a Chromium page field does not select over autocompletion", !ngosen::selectsOverAutocompletion(chrome));
        chrome.caretWidth  = 1;
        chrome.caretHeight = 17;
        check("the Chromium address bar selects over autocompletion", ngosen::selectsOverAutocompletion(chrome));
        auto vivaldi             = field("dbus", "vivaldi-stable");
        vivaldi.keyEventOrderFix = true;
        vivaldi.caretHeight      = 18;
        check("a Vivaldi field without a selection does not select over autocompletion", !ngosen::selectsOverAutocompletion(vivaldi));
        vivaldi.caretWidth = 73;
        check("the Vivaldi address bar autocompletion is selected over", ngosen::selectsOverAutocompletion(vivaldi));
        auto terminal             = field("dbus", "gnome-terminal-server");
        terminal.keyEventOrderFix = true;
        check("a terminal does not select over autocompletion", !ngosen::selectsOverAutocompletion(terminal));
        check("IBus surrounding text lags", ngosen::surroundingTextLags(field("ibus", "x")));
        check("LibreOffice applies backspaces late", ngosen::appliesBackspacesLate(field("wayland", "soffice.desktop")));
        check("Firefox hides the address bar flag", ngosen::hidesAddressBarFlag(field("wayland", "firefox")));
        check("Firefox ESR hides the address bar flag", ngosen::hidesAddressBarFlag(field("dbus", "firefox-esr")));
        check("Zen hides the address bar flag", ngosen::hidesAddressBarFlag(field("dbus", "zen")));
        check("LibreWolf hides the address bar flag", ngosen::hidesAddressBarFlag(field("wayland", "librewolf.desktop")));
        check("Waterfox hides the address bar flag", ngosen::hidesAddressBarFlag(field("dbus", "waterfox")));
        check("zenity is not a browser", !ngosen::hidesAddressBarFlag(field("dbus", "zenity")));
    }

    // Desktop files the lookup finds through XDG_DATA_DIRS: Flatpak exports, and a native app with a reverse-DNS name.
    void writeDesktopFiles(const std::filesystem::path& root) {
        const auto write = [](const std::filesystem::path& dir, const std::string& id, const std::string& text) {
            std::filesystem::create_directories(dir);
            std::ofstream(dir / (id + ".desktop")) << text;
        };
        const auto flatpak = root / "flatpak/exports/share/applications";
        write(flatpak, "org.mozilla.firefox", "[Desktop Entry]\r\nName=Firefox\r\nStartupWMClass = firefox \r\n\r\n[Desktop Action new-window]\r\nStartupWMClass=other\r\n");
        write(flatpak, "net.mullvad.MullvadBrowser", "[Desktop Entry]\nStartupWMClass=Mullvad Browser\n");
        write(flatpak, "net.waterfox.waterfox", "[Desktop Entry]\nName=Waterfox\n");
        write(flatpak, "net.lutris.Lutris", "[Desktop Entry]\nStartupWMClass=net.lutris.Lutris\n");
        write(root / "usr/share/applications", "org.gnome.Ptyxis", "[Desktop Entry]\nStartupWMClass=ptyxis\n");
        setenv("XDG_DATA_DIRS", ((root / "flatpak/exports/share").string() + ":" + (root / "usr/share").string()).c_str(), 1);
    }

    void checkFlatpakNames(TestInstance& testInstance) {
        TestInputContext  firefox(&testInstance.instance, "org.mozilla.firefox", "dbus");
        ngosen::FcitxHost host(&firefox, &testInstance.instance);
        const auto        f = host.field();
        check("Flatpak Firefox hides the address bar flag", ngosen::hidesAddressBarFlag(f));
        check("the reported name is kept beside it", f.program == "firefox" && f.reportedProgram == "org.mozilla.firefox");
        check("a StartupWMClass with spaces drops them", ngosen::normalizeProgramName("net.mullvad.MullvadBrowser") == "mullvadbrowser");
        check("without StartupWMClass the last part names it", ngosen::normalizeProgramName("net.waterfox.waterfox") == "waterfox");
        check("a StartupWMClass that is the app ID is skipped", ngosen::normalizeProgramName("net.lutris.Lutris") == "lutris");
        check("the GNOME desktop ID of a Flatpak app is mapped", ngosen::normalizeProgramName("org.mozilla.firefox.desktop") == "firefox");
        check("a native app with a reverse-DNS name is left as reported", ngosen::normalizeProgramName("org.gnome.Ptyxis") == "org.gnome.Ptyxis");
        check("a name with no desktop file is left as reported", ngosen::normalizeProgramName("com.example.Operator") == "com.example.Operator");
        TestInputContext gedit(&testInstance.instance, "gedit", "dbus");
        check("a native name is not marked as mapped", ngosen::FcitxHost(&gedit, &testInstance.instance).field().reportedProgram.empty());
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-host-field-mapping");
    // Before fcitx5 reads the data directories.
    writeDesktopFiles(std::filesystem::temp_directory_path() / "fcitx5-ngosen-host-field-mapping" / "system");
    TestInstance testInstance;
    checkFlags(testInstance);
    checkShellDropsKeys(testInstance);
    checkShellFocusedApp(testInstance);
    checkSurrounding(testInstance);
    checkFocus(testInstance);
    checkPreedit(testInstance);
    checkKeys(testInstance);
    checkQuirks();
    checkFlatpakNames(testInstance);
    return failures == 0 ? 0 : 1;
}
