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
} // namespace

int main() {
    using ngosen::focusedGnomeApp;
    const std::vector<ngosen::RunningApp> twoApps = {{"org.gnome.TextEditor.desktop", false}, {"org.gnome.Nautilus.desktop", true}};
    expect("the app with active-on-seats is focused", focusedGnomeApp(twoApps, false), "org.gnome.Nautilus.desktop");
    expect("the name keeps .desktop, as saved rules have it", focusedGnomeApp({{"firefox_firefox.desktop", true}}, false), "firefox_firefox.desktop");
    expect("the search box wins over the app behind it", focusedGnomeApp(twoApps, true), ngosen::gnomeOverviewApp);
    expect("the search box with no running apps", focusedGnomeApp({}, true), ngosen::gnomeOverviewApp);
    expect("a window without a .desktop file has no name", focusedGnomeApp({{"window:617", true}}, false), "");
    expect("no focused app has no name", focusedGnomeApp({{"org.gnome.TextEditor.desktop", false}}, false), "");
    expect("no apps at all has no name", focusedGnomeApp({}, false), "");
    if (failures == 0)
        std::cout << "core_gnome_apps: all passed\n";
    return failures == 0 ? 0 : 1;
}
