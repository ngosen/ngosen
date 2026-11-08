// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-gnome-apps.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
    int  failures = 0;

    void expect(const std::string& name, const std::string& got, const std::string& want) {
        if (got == want)
            return;
        std::cerr << "FAIL: " << name << ": got '" << got << "', want '" << want << "'\n";
        ++failures;
    }

    void expectTrue(const std::string& name, bool ok) {
        expect(name, ok ? "true" : "false", "true");
    }

    void testFocusedApp() {
        using ngosen::focusedGnomeApp;
        const std::vector<ngosen::RunningApp> twoApps = {{"org.gnome.TextEditor.desktop", false}, {"org.gnome.Nautilus.desktop", true}};
        expect("the app with active-on-seats is focused", focusedGnomeApp(twoApps, false), "org.gnome.Nautilus.desktop");
        expect("the name keeps .desktop, as saved rules have it", focusedGnomeApp({{"firefox_firefox.desktop", true}}, false), "firefox_firefox.desktop");
        expect("the search box wins over the app behind it", focusedGnomeApp(twoApps, true), ngosen::gnomeOverviewApp);
        expect("the search box with no running apps", focusedGnomeApp({}, true), ngosen::gnomeOverviewApp);
        expect("a window without a .desktop file has no name", focusedGnomeApp({{"window:617", true}}, false), "");
        expect("no focused app has no name", focusedGnomeApp({{"org.gnome.TextEditor.desktop", false}}, false), "");
        expect("no apps at all has no name", focusedGnomeApp({}, false), "");
    }

    void testReply() {
        const auto apps = ngosen::runningApps({{"org.gnome.TextEditor.desktop", {}}, {"org.gnome.Nautilus.desktop", {"seat0"}}});
        expect("reply: an app without active-on-seats", ngosen::focusedGnomeApp(apps, false), "org.gnome.Nautilus.desktop");
        expect("reply: an empty active-on-seats is not active", ngosen::focusedGnomeApp(ngosen::runningApps({{"org.gnome.Nautilus.desktop", {}}}), false), "");
    }

    void testMatcher() {
        const std::string              shell = ":1.22", portal = ":1.71", sig = "a{sa{sv}}";
        ngosen::IntrospectReplyMatcher matcher;
        matcher.setOwners(shell, portal);
        expectTrue("match: no reply counts before a call", !matcher.answers(shell, portal, 7, sig));
        matcher.call(portal, shell, 7);
        expectTrue("match: a reply from another sender", !matcher.answers(":1.99", portal, 7, sig));
        expectTrue("match: a reply to someone else", !matcher.answers(shell, ":1.99", 7, sig));
        expectTrue("match: a reply of another type", !matcher.answers(shell, portal, 7, "a{ta{sv}}"));
        expectTrue("match: the reply to the call", matcher.answers(shell, portal, 7, sig));
        expectTrue("match: the same reply only once", !matcher.answers(shell, portal, 7, sig));

        matcher.call(portal, shell, 8);
        matcher.call(portal, shell, 9);
        expectTrue("match: a reply to an older call", !matcher.answers(shell, portal, 8, sig));
        expectTrue("match: the reply to the newest call", matcher.answers(shell, portal, 9, sig));

        matcher.call(portal, shell, 10);
        matcher.setOwners(":1.300", portal);
        expectTrue("match: a call to GNOME Shell before it restarted", !matcher.answers(shell, portal, 10, sig));
        expectTrue("match: the new GNOME Shell's reply needs a call", !matcher.answers(":1.300", portal, 10, sig));

        // fcitx5 on libdbus does not tell serials.
        ngosen::IntrospectReplyMatcher noSerials;
        noSerials.setOwners(shell, portal);
        noSerials.call(portal, shell, 0);
        expectTrue("no serials: a reply from GNOME Shell to the portal", noSerials.answers(shell, portal, 0, sig));
        expectTrue("no serials: replies to quick calls, in order", noSerials.answers(shell, portal, 0, sig));
        expectTrue("no serials: another type", !noSerials.answers(shell, portal, 0, "v"));
        noSerials.setOwners(shell, ":1.80");
        expectTrue("no serials: a reply to the portal that left", !noSerials.answers(shell, portal, 0, sig));
    }

    void testNames() {
        const std::vector<ngosen::RunningApp> editor = {{"org.gnome.TextEditor.desktop", true}, {"org.gnome.Nautilus.desktop", false}};
        const std::vector<ngosen::RunningApp> files  = {{"org.gnome.TextEditor.desktop", false}, {"org.gnome.Nautilus.desktop", true}};
        ngosen::GnomeAppNames                 names;
        expectTrue("names: the portal's app", names.setPortalApps(editor));
        expect("names: the portal's app", names.name(), "org.gnome.TextEditor.desktop");
        expectTrue("names: the same app is no change", !names.setPortalApps(editor));

        names.setExtensionApp("nodesk.py");
        expect("names: the extension wins", names.name(), "nodesk.py");
        names.setPortalApps(files);
        expect("names: the extension still wins", names.name(), "nodesk.py");
        names.setExtensionRunning(false);
        expect("names: back to the portal when the extension stops", names.name(), "org.gnome.Nautilus.desktop");
        names.setExtensionRunning(true);
        expect("names: a restarted extension's old name is gone", names.name(), "org.gnome.Nautilus.desktop");

        names.setOverviewActive(true);
        expect("names: the overview", names.name(), ngosen::gnomeOverviewApp);
        names.setPortalApps(files);
        expect("names: a reply for the same app keeps the overview", names.name(), ngosen::gnomeOverviewApp);
        names.setOverviewActive(false);
        expect("names: the app after the overview", names.name(), "org.gnome.Nautilus.desktop");

        names.setOverviewActive(true);
        names.setPortalApps(editor);
        expect("names: picking another app ends the overview", names.name(), "org.gnome.TextEditor.desktop");
        expectTrue("names: OverviewActive going false then is no change", !names.setOverviewActive(false));

        expectTrue("names: GNOME Shell or the portal leaving drops the name", names.dropPortalApp());
        expect("names: GNOME Shell or the portal leaving drops the name", names.name(), "");
    }
} // namespace

int main() {
    testFocusedApp();
    testReply();
    testMatcher();
    testNames();
    if (failures == 0)
        std::cout << "core_gnome_apps: all passed\n";
    return failures == 0 ? 0 : 1;
}
