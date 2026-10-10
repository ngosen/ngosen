// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit regression test for isPlasmaPanelDark() (issue #374).
//
// Each case builds a throwaway XDG tree (config + data directories) that
// mirrors a real KDE setup, then asks whether the panel is dark.  The first
// case is the reported bug: the default Fedora 44 look ships a light
// application colour scheme and the breeze-dark Plasma Style, stored in
// ~/.config/kdedefaults, so the tray icon must be the white one.
//
#include "ngosen-plasma-theme.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <unistd.h>

namespace {

    namespace fs = std::filesystem;

    const char* const kDark  = "32,35,38";    // Breeze Dark window background
    const char* const kLight = "239,240,241"; // Breeze Light window background

    int               failures = 0;

    void              writeFile(const fs::path& path, const std::string& content) {
        fs::create_directories(path.parent_path());
        std::ofstream(path) << content;
    }

    std::string colorsFile(const char* background) {
        return std::string("[Colors:Window]\nBackgroundAlternate=1,2,3\nBackgroundNormal=") + background + "\nForegroundNormal=9,9,9\n";
    }

    std::string show(std::optional<bool> value) {
        if (!value)
            return "nullopt";
        return *value ? "dark" : "light";
    }

    void check(const char* name, bool ok, const std::string& detail) {
        std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name, ok ? "" : " — ", ok ? "" : detail.c_str());
        if (!ok)
            ++failures;
    }

    // One isolated XDG tree per case.
    struct Tree {
        fs::path                      root;
        fcitx::PlasmaThemeSearchPaths paths;

        explicit Tree(const char* name) : root(fs::temp_directory_path() / ("ngosen-plasma-theme-test-" + std::to_string(getpid()) + "-" + name)) {
            fs::remove_all(root);
            paths.configHome = (root / "config").string();
            paths.configDirs = {(root / "config/kdedefaults").string(), (root / "etc/xdg").string()};
            paths.dataHome   = (root / "local/share").string();
            paths.dataDirs   = {(root / "usr/share").string()};
            // Themes installed on every KDE system.
            writeFile(root / "usr/share/plasma/desktoptheme/default/metadata.json", "{}");
            writeFile(root / "usr/share/plasma/desktoptheme/breeze-dark/colors", colorsFile(kDark));
            writeFile(root / "usr/share/plasma/desktoptheme/breeze-light/colors", colorsFile(kLight));
        }
        ~Tree() {
            fs::remove_all(root);
        }
        void write(const char* relative, const std::string& content) {
            writeFile(root / relative, content);
        }
    };

    void expect(const char* name, Tree& tree, std::optional<bool> expected) {
        const auto actual = fcitx::isPlasmaPanelDark(tree.paths);
        check(name, actual == expected, "expected " + show(expected) + ", got " + show(actual));
    }

} // namespace

int main() {
    {
        // The #374 report: light apps, dark panel, set by the Global Theme.
        Tree t("fedora-default");
        t.write("config/kdedefaults/plasmarc", "[Theme]\nname=breeze-dark\n");
        t.write("config/kdeglobals", colorsFile(kLight));
        expect("P1 fedora_default_light_apps_dark_panel", t, true);
    }
    {
        Tree t("breeze-light-scheme");
        t.write("config/plasmarc", "[Theme]\nname=default\n");
        t.write("config/kdeglobals", colorsFile(kLight));
        expect("P2 breeze_default_follows_light_scheme", t, false);
    }
    {
        Tree t("breeze-dark-scheme");
        t.write("config/plasmarc", "[Theme]\nname=default\n");
        t.write("config/kdeglobals", colorsFile(kDark));
        expect("P3 breeze_default_follows_dark_scheme", t, true);
    }
    {
        Tree t("light-panel-dark-apps");
        t.write("config/plasmarc", "[Theme]\nname=breeze-light\n");
        t.write("config/kdeglobals", colorsFile(kDark));
        expect("P4 light_panel_with_dark_apps", t, false);
    }
    {
        Tree t("user-overrides-defaults");
        t.write("config/kdedefaults/plasmarc", "[Theme]\nname=breeze-dark\n");
        t.write("config/plasmarc", "[Theme]\nname=breeze-light\n");
        t.write("config/kdeglobals", colorsFile(kDark));
        expect("P5 user_plasmarc_overrides_global_theme", t, false);
    }
    {
        Tree t("user-theme-shadows-system");
        t.write("config/plasmarc", "[Theme]\nname=breeze-light\n");
        t.write("local/share/plasma/desktoptheme/breeze-light/colors", colorsFile(kDark));
        t.write("config/kdeglobals", colorsFile(kLight));
        expect("P6 user_installed_theme_shadows_system", t, true);
    }
    {
        // A user copy without a colors file follows the scheme, even though
        // the system copy of the same name has dark colours.
        Tree t("user-theme-no-colors");
        t.write("config/plasmarc", "[Theme]\nname=breeze-dark\n");
        t.write("local/share/plasma/desktoptheme/breeze-dark/metadata.json", "{}");
        t.write("config/kdeglobals", colorsFile(kLight));
        expect("P7 user_theme_without_colors_follows_scheme", t, false);
    }
    {
        Tree t("missing-theme");
        t.write("config/plasmarc", "[Theme]\nname=not-installed\n");
        t.write("config/kdeglobals", colorsFile(kDark));
        expect("P8 missing_theme_follows_scheme", t, true);
    }
    {
        Tree t("scheme-name-only");
        t.write("config/kdedefaults/kdeglobals", "[General]\nColorScheme=BreezeDark\n");
        t.write("usr/share/color-schemes/BreezeDark.colors", colorsFile(kDark));
        expect("P9 scheme_name_only_resolves_colors_file", t, true);
    }
    {
        Tree t("nothing-known");
        expect("P10 nothing_known", t, std::nullopt);
    }

    check("S1 kde_session", fcitx::isKdePlasmaSession("KDE"), "KDE not recognised");
    check("S2 gnome_session", !fcitx::isKdePlasmaSession("GNOME"), "GNOME taken for KDE");
    check("S3 no_desktop", !fcitx::isKdePlasmaSession(""), "empty desktop taken for KDE");

    {
        setenv("HOME", "/h", 1);
        setenv("XDG_CONFIG_HOME", "/c", 1);
        setenv("XDG_CONFIG_DIRS", "/etc/xdg", 1);
        const auto p  = fcitx::plasmaThemeSearchPathsFromEnv();
        const bool ok = p.configDirs.size() == 2 && p.configDirs[0] == "/c/kdedefaults" && p.configDirs[1] == "/etc/xdg";
        check("E1 env_adds_kdedefaults", ok, "configDirs lacks /c/kdedefaults in front of /etc/xdg");
    }
    {
        setenv("HOME", "/h", 1);
        unsetenv("XDG_CONFIG_HOME");
        unsetenv("XDG_CONFIG_DIRS");
        unsetenv("XDG_DATA_HOME");
        unsetenv("XDG_DATA_DIRS");
        const auto p = fcitx::plasmaThemeSearchPathsFromEnv();
        const bool ok =
            p.configHome == "/h/.config" && p.dataHome == "/h/.local/share" && p.dataDirs.size() == 2 && p.dataDirs[0] == "/usr/local/share" && p.dataDirs[1] == "/usr/share";
        check("E2 env_xdg_defaults", ok, "XDG defaults not applied");
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
