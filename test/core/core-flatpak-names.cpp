// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-flatpak-names.h"

#include <iostream>
#include <string>

namespace {
    int  failures = 0;

    void expect(const std::string& name, const std::string& got, const std::string& want) {
        if (got == want)
            return;
        std::cerr << "FAIL: " << name << ": got '" << got << "', want '" << want << "'\n";
        ++failures;
    }

    void expect(const std::string& name, bool got, bool want) {
        expect(name, std::string(got ? "true" : "false"), std::string(want ? "true" : "false"));
    }
} // namespace

int main() {
    using namespace ngosen;
    expect("an app ID is a candidate", flatpakAppIdCandidate("org.mozilla.firefox"), "org.mozilla.firefox");
    expect("GNOME's .desktop suffix is dropped", flatpakAppIdCandidate("org.mozilla.firefox.desktop"), "org.mozilla.firefox");
    expect("a plain program name is not a candidate", flatpakAppIdCandidate("firefox"), "");
    expect("one dot is not an app ID", flatpakAppIdCandidate("org.firefox"), "");
    expect("a path is not an app ID", flatpakAppIdCandidate("/usr/lib/a.b.c"), "");

    expect("a system Flatpak export", isFlatpakExport("/var/lib/flatpak/exports/share/applications/org.mozilla.firefox.desktop"), true);
    expect("a user Flatpak export", isFlatpakExport("/home/u/.local/share/flatpak/exports/share/applications/a.b.c.desktop"), true);
    expect("a native desktop file", isFlatpakExport("/usr/share/applications/org.gnome.Ptyxis.desktop"), false);

    const std::string firefox = "firefox", mullvad = "Mullvad Browser", lutris = "net.lutris.Lutris", empty;
    expect("StartupWMClass names the app", programForFlatpakApp("org.mozilla.firefox", &firefox), "firefox");
    expect("a StartupWMClass with spaces drops them", programForFlatpakApp("net.mullvad.MullvadBrowser", &mullvad), "mullvadbrowser");
    expect("without StartupWMClass the last part names it", programForFlatpakApp("net.waterfox.waterfox", nullptr), "waterfox");
    expect("an empty StartupWMClass is skipped", programForFlatpakApp("net.waterfox.Waterfox", &empty), "waterfox");
    expect("a StartupWMClass that is the app ID is skipped", programForFlatpakApp("net.lutris.Lutris", &lutris), "lutris");

    if (failures == 0)
        std::cout << "core_flatpak_names: all passed\n";
    return failures == 0 ? 0 : 1;
}
