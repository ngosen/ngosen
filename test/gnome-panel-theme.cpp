// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit regression test for isGnomePanelDark(), the GNOME twin of #374.
//
// The reported machine runs Ubuntu 24.04 with the WhiteSur-Dark shell theme
// through the User Themes extension and color-scheme 'default': the portal
// says light, so the engine drew a black icon, while the top bar is translucent
// with white text.  Ubuntu's own Yaru bar is near-black under the light
// scheme too.  The CSS snippets below are copied from those themes.
//
#include "ngosen-gnome-theme.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <unistd.h>

namespace {

    namespace fs = std::filesystem;

    int  failures = 0;

    void writeFile(const fs::path& path, const std::string& content) {
        fs::create_directories(path.parent_path());
        std::ofstream(path) << content;
    }

    std::string show(std::optional<bool> value) {
        if (!value)
            return "nullopt";
        return *value ? "dark" : "light";
    }

    void expectCss(const char* name, const std::string& css, std::optional<bool> expected) {
        const auto got = fcitx::isShellCssPanelDark(css);
        if (got != expected) {
            std::printf("FAIL %s: expected %s, got %s\n", name, show(expected).c_str(), show(got).c_str());
            ++failures;
        }
    }

    void check(const char* name, bool ok, const char* what) {
        if (!ok) {
            std::printf("FAIL %s: %s\n", name, what);
            ++failures;
        }
    }

    const char* const kWhiteSurDark  = "stage { color: black; }\n"
                                       "#panel {\n  background-color: rgba(0, 0, 0, 0.15);\n  font-weight: 500;\n  color: white;\n"
                                       "  height: 28px !important;\n  box-shadow: 0 5px 16px rgba(0, 0, 0, 0.05);\n}\n"
                                       "#panel .panel-button { color: black; }\n#panel.solid { background-color: #fff; }\n";
    const char* const kWhiteSurLight = "#panel {\n  background-color: rgba(255, 255, 255, 0.155);\n  color: white;\n}\n";
    const char* const kYaru          = ".x { color: #000; }\n #panel {\n  background-color: #131313;\n  font-weight: bold;\n  height: 2.2em;\n  transition-duration: 250ms; }\n";

} // namespace

int main() {
    // ── Reading the #panel rule ──────────────────────────────────────────
    expectCss("C1 whitesur_dark", kWhiteSurDark, true);
    expectCss("C2 whitesur_light_white_text", kWhiteSurLight, true);
    expectCss("C3 yaru_opaque_dark", kYaru, true);
    expectCss("C4 opaque_light", "#panel { background-color: #f6f5f4; }", false);
    expectCss("C5 dark_text", "#panel { background-color: rgba(0,0,0,0.1); color: #2e3436; }", false);
    expectCss("C6 descendants_only", "#panel .panel-button { color: white; }\n#panel.solid { background-color: black; }", std::nullopt);
    expectCss("C7 commented_out", "/* #panel { color: white; } */", std::nullopt);
    expectCss("C8 later_rule_wins", "#panel { color: black; }\n#panel { color: #fff; }", true);
    expectCss("C9 translucent_background_only", "#panel { background-color: rgba(0, 0, 0, 0.15); }", std::nullopt);
    expectCss("C10 grouped_selector", "#foo, #panel { color: white; }", std::nullopt);

    // ── Which desktops count ─────────────────────────────────────────────
    check("D1 ubuntu", fcitx::isGnomeShellSession("ubuntu:GNOME"), "ubuntu:GNOME rejected");
    check("D2 gnome", fcitx::isGnomeShellSession("GNOME"), "GNOME rejected");
    check("D3 budgie", !fcitx::isGnomeShellSession("Budgie:GNOME"), "Budgie taken for GNOME Shell");
    check("D4 kde", !fcitx::isGnomeShellSession("KDE"), "KDE taken for GNOME Shell");
    check("D5 empty", !fcitx::isGnomeShellSession(""), "empty desktop taken for GNOME Shell");

    // ── Finding the stylesheet ───────────────────────────────────────────
    char        tmpl[] = "/tmp/ngosen-gnome-theme-XXXXXX";
    const char* dir    = mkdtemp(tmpl);
    if (dir == nullptr) {
        std::printf("FAIL mkdtemp\n");
        return 1;
    }
    const fs::path             root(dir);

    fcitx::GnomeShellThemeInfo info;
    info.home     = (root / "home").string();
    info.dataHome = (root / "home/.local/share").string();
    info.dataDirs = {(root / "usr/share/ubuntu").string(), (root / "usr/share").string()};

    writeFile(root / "usr/share/gnome-shell/modes/ubuntu.json",
              "{\n    \"parentMode\": \"user\",\n    \"stylesheetName\": \"Yaru/gnome-shell.css\",\n    \"colorScheme\": \"prefer-light\"\n}\n");
    writeFile(root / "usr/share/gnome-shell/theme/Yaru/gnome-shell.css", kYaru);
    writeFile(root / "home/.themes/WhiteSur-Dark/gnome-shell/gnome-shell.css", kWhiteSurDark);
    writeFile(root / "usr/share/gnome-shell/theme/Mode-Theme.css", "#panel { color: black; }");

    info.userThemeName = "WhiteSur-Dark";
    info.sessionMode   = "ubuntu";
    auto path          = fcitx::gnomeShellStylesheet(info);
    check("S1 user_theme_home", path && *path == (root / "home/.themes/WhiteSur-Dark/gnome-shell/gnome-shell.css").string(), "user theme in ~/.themes not found");
    check("E1 reported_machine", fcitx::isGnomePanelDark(info) == std::optional<bool>(true), "WhiteSur-Dark on Ubuntu not dark");

    info.userThemeName = "Not-Installed";
    path               = fcitx::gnomeShellStylesheet(info);
    check("S2 missing_theme_falls_to_mode", path && *path == (root / "usr/share/gnome-shell/theme/Yaru/gnome-shell.css").string(),
          "missing user theme did not fall back to the Yaru mode stylesheet");

    info.userThemeName = "Mode-Theme";
    path               = fcitx::gnomeShellStylesheet(info);
    check("S3 theme_in_shell_theme_dir", path && *path == (root / "usr/share/gnome-shell/theme/Mode-Theme.css").string(), "gnome-shell/theme/<name>.css not found");

    info.userThemeName = "";
    info.sessionMode   = "user";
    check("S4 plain_gnome_unknown", !fcitx::gnomeShellStylesheet(info).has_value(), "plain GNOME returned a stylesheet");

    // ── Built-in Adwaita bar: GNOME 51 follows color-scheme ──────────────
    info.shellMajorVersion = 51;
    info.colorScheme       = "prefer-light";
    check("G1 gnome51_prefer_light", fcitx::isGnomePanelDark(info) == std::optional<bool>(false), "GNOME 51 prefer-light bar not light");
    info.colorScheme = "default";
    check("G2 gnome51_default_dark", fcitx::isGnomePanelDark(info) == std::optional<bool>(true), "GNOME 51 default bar not dark");
    info.colorScheme = "prefer-dark";
    check("G3 gnome51_prefer_dark", fcitx::isGnomePanelDark(info) == std::optional<bool>(true), "GNOME 51 prefer-dark bar not dark");
    info.colorScheme = "";
    check("G4 gnome51_unknown_scheme", fcitx::isGnomePanelDark(info) == std::optional<bool>(true), "GNOME 51 without a scheme not dark");
    info.colorScheme       = "prefer-light";
    info.shellMajorVersion = 50;
    check("G5 gnome50_unmeasured", !fcitx::isGnomePanelDark(info).has_value(), "GNOME 50 answered from color-scheme");
    info.shellMajorVersion = 0;
    check("G6 version_unknown", !fcitx::isGnomePanelDark(info).has_value(), "unknown version answered from color-scheme");
    info.shellMajorVersion = 52;
    info.sessionMode       = "";
    check("G7 empty_mode_is_user", fcitx::isGnomePanelDark(info) == std::optional<bool>(false), "empty session mode not treated as user");
    info.sessionMode = "ubuntu";
    check("G8 ubuntu_mode_uses_yaru", fcitx::isGnomePanelDark(info) == std::optional<bool>(true), "Ubuntu Yaru bar decided by color-scheme");
    info.sessionMode = "user";
    check("V1 version_line", fcitx::gnomeShellMajorVersion("GNOME Shell 51.1") == 51, "GNOME Shell 51.1 not read as 51");
    check("V2 version_alpha", fcitx::gnomeShellMajorVersion("GNOME Shell 52.alpha") == 52, "52.alpha not read as 52");
    check("V3 version_missing", fcitx::gnomeShellMajorVersion("") == 0, "empty line not 0");

    // ── Cinnamon: the panel follows the Cinnamon theme, not the GTK one ──
    // Linux Mint 22.1 pairs Mint-Y-Dark-Aqua for the panel with Mint-Y-Aqua for
    // apps, so the portal says light while the panel is dark.  Rules copied from it.
    check("N1 mint", fcitx::isCinnamonSession("", "cinnamon"), "DESKTOP_SESSION=cinnamon rejected");
    check("N2 x_cinnamon", fcitx::isCinnamonSession("X-Cinnamon", ""), "X-Cinnamon rejected");
    check("N3 gnome", !fcitx::isCinnamonSession("ubuntu:GNOME", "ubuntu"), "GNOME taken for Cinnamon");

    check("T1 cinnamon_tray_by_name", fcitx::trayWantsIconName("", "cinnamon"), "Cinnamon tray given a path");
    check("T2 cosmic_tray_by_name", fcitx::trayWantsIconName("COSMIC", ""), "COSMIC tray given a path");
    check("T3 kde_tray_by_path", !fcitx::trayWantsIconName("KDE", "plasma"), "KDE tray given a name");
    check("T4 gnome_tray_tighter_icon", fcitx::trayIconNames("fcitx-ngosen", "fcitx-ngosen", "ubuntu:GNOME").front() == "fcitx-ngosen-gnome",
          "GNOME Shell not given the GNOME icon first");
    check("T5 kde_tray_usual_icon", fcitx::trayIconNames("fcitx-ngosen-off", "fcitx-ngosen-off", "KDE").front() == "fcitx-ngosen-off", "KDE given the GNOME icon");

    writeFile(root / "usr/share/themes/Mint-Y-Dark-Aqua/cinnamon/cinnamon.css",
              "#panel {\n  font-weight: bold;\n  height: 40px;\n  color: #e1e1e1; }\n"
              ".panel-top, .panel-bottom { color: #ffffff; background-color: rgba(29, 29, 33, 0.99); }\n");
    writeFile(root / "home/.themes/Mint-Y-Aqua/cinnamon/cinnamon.css", "#panel {\n  font-weight: bold;\n  color: #303030; }\n");
    writeFile(root / "usr/share/cinnamon/theme/cinnamon.css", "#panel { font-weight: bold; }\n");

    fcitx::CinnamonThemeInfo mint;
    mint.home      = info.home;
    mint.dataHome  = info.dataHome;
    mint.dataDirs  = info.dataDirs;
    mint.themeName = "Mint-Y-Dark-Aqua";
    check("N4 mint_default_dark", fcitx::isCinnamonPanelDark(mint) == std::optional<bool>(true), "Mint-Y-Dark-Aqua panel not dark");
    mint.themeName = "Mint-Y-Aqua";
    check("N5 light_theme_in_home", fcitx::isCinnamonPanelDark(mint) == std::optional<bool>(false), "Mint-Y-Aqua in ~/.themes not light");
    mint.themeName = "";
    path           = fcitx::cinnamonStylesheet(mint);
    check("N6 builtin_theme", path && *path == (root / "usr/share/cinnamon/theme/cinnamon.css").string(), "built-in Cinnamon theme not found");
    check("N7 builtin_says_nothing", !fcitx::isCinnamonPanelDark(mint).has_value(), "built-in theme without a panel colour gave an answer");
    mint.themeName = "Not-Installed";
    check("N8 missing_theme", !fcitx::isCinnamonPanelDark(mint).has_value(), "missing Cinnamon theme gave an answer");

    fs::remove_all(root);

    if (failures == 0)
        std::printf("gnome_panel_theme: all cases passed\n");
    return failures == 0 ? 0 : 1;
}
