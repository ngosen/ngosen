// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-flatpak-names.h"

#include <iostream>
#include <map>
#include <optional>
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

    const std::string systemExports = "/var/lib/flatpak/exports/share/applications/";
    const std::string userExports   = "/home/u/.local/share/flatpak/exports/share/applications/";
    const std::string native        = "/usr/share/applications/";

    // Desktop files by path, with the StartupWMClass each holds; nullopt for one that cannot be read.
    struct FakeDataDirs {
        std::vector<std::string>                          order = {userExports, systemExports, native};
        std::map<std::string, std::optional<std::string>> files;
        std::vector<std::string>                          read;

        std::string                                       map(const std::string& program) {
            read.clear();
            const auto find = [this](const std::string& appId) {
                std::vector<std::string> found;
                for (const auto& dir : order)
                    if (files.count(dir + appId + ".desktop"))
                        found.push_back(dir + appId + ".desktop");
                return found;
            };
            const auto readWmClass = [this](const std::string& path) {
                read.push_back(path);
                return files.at(path);
            };
            return ngosen::flatpakProgramName(program, find, readWmClass);
        }
    };
} // namespace

int main() {
    FakeDataDirs dirs;
    dirs.files[systemExports + "org.mozilla.firefox.desktop"]        = std::string("firefox");
    dirs.files[systemExports + "net.mullvad.MullvadBrowser.desktop"] = std::string("Mullvad Browser");
    dirs.files[systemExports + "net.waterfox.waterfox.desktop"]      = std::string();
    dirs.files[systemExports + "net.lutris.Lutris.desktop"]          = std::string("net.lutris.Lutris");
    dirs.files[systemExports + "com.example.Padded.desktop"]         = std::string("\tpadded\r");
    dirs.files[native + "org.gnome.Ptyxis.desktop"]                  = std::string("ptyxis");

    expect("StartupWMClass names the app", dirs.map("org.mozilla.firefox"), "firefox");
    expect("GNOME's .desktop suffix is dropped before the lookup", dirs.map("org.mozilla.firefox.desktop"), "firefox");
    expect("a StartupWMClass with spaces drops them", dirs.map("net.mullvad.MullvadBrowser"), "mullvadbrowser");
    expect("without StartupWMClass the last part names it", dirs.map("net.waterfox.waterfox"), "waterfox");
    expect("the last part of a .desktop id is not \"desktop\"", dirs.map("net.waterfox.waterfox.desktop"), "waterfox");
    expect("a StartupWMClass that is the app ID is skipped", dirs.map("net.lutris.Lutris"), "lutris");
    expect("whitespace a parser leaves is dropped", dirs.map("com.example.Padded"), "padded");
    expect("a native app with a reverse-DNS name is left as reported", dirs.map("org.gnome.Ptyxis"), "org.gnome.Ptyxis");
    expect("a native desktop file is never read", std::to_string(dirs.read.size()), "0");
    expect("a name with no desktop file is left as reported", dirs.map("com.example.Operator"), "com.example.Operator");
    expect("a plain program name is left as reported", dirs.map("firefox"), "firefox");
    expect("a path is not an app ID", dirs.map("/usr/lib/a.b.c"), "/usr/lib/a.b.c");

    dirs.files[userExports + "org.mozilla.firefox.desktop"] = std::string("user-firefox");
    expect("the first export on the data path wins", dirs.map("org.mozilla.firefox"), "user-firefox");
    // The class differs from the ID's last part, so mapping the unreadable file by its ID shows.
    dirs.files[userExports + "com.example.Viewer.desktop"]   = std::nullopt;
    dirs.files[systemExports + "com.example.Viewer.desktop"] = std::string("imgview");
    expect("an unreadable export falls through to the next", dirs.map("com.example.Viewer"), "imgview");

    if (failures == 0)
        std::cout << "core_flatpak_names: all passed\n";
    return failures == 0 ? 0 : 1;
}
