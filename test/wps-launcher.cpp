// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file wps-launcher.cpp
 * @brief Unit test: on Plasma Wayland, WPS launchers get copies that start WPS with fcitx5's Qt
 * module, so WPS types without the user setting QT_IM_MODULE.
 */

#include "ngosen-wps-launcher.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    // As WPS 12's deb installs it, trimmed.
    const std::string Spreadsheets = "[Desktop Entry]\n"
                                     "Comment=Use WPS Spreadsheets to analyze manage data.\n"
                                     "Exec=/usr/bin/et %F\n"
                                     "Name=WPS Spreadsheets\n"
                                     "Type=Application\n"
                                     "StartupWMClass=et\n";

    void              write(const std::filesystem::path& path, const std::string& text) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << text;
    }

    std::string read(const std::filesystem::path& path) {
        std::ifstream      in(path);
        std::ostringstream text;
        text << in.rdbuf();
        return text.str();
    }

    void testSession() {
        check("Plasma Wayland gets it", ngosen::wantsWpsInputMethod("KDE", "wayland"));
        check("a desktop list naming KDE gets it", ngosen::wantsWpsInputMethod("ubuntu:KDE", "wayland"));
        check("Plasma on X11 does not", !ngosen::wantsWpsInputMethod("KDE", "x11"));
        check("GNOME does not", !ngosen::wantsWpsInputMethod("ubuntu:GNOME", "wayland"));
        check("a name only containing KDE does not", !ngosen::wantsWpsInputMethod("KDEx", "wayland"));
    }

    void testLauncher() {
        const auto launcher = ngosen::wpsLauncherWithInputMethod(Spreadsheets);
        check("a launcher with Exec is rewritten", launcher.has_value());
        if (!launcher)
            return;
        check("Exec starts WPS with fcitx5's Qt module", launcher->find("\nExec=env QT_IM_MODULE=fcitx /usr/bin/et %F\n") != std::string::npos);
        check("TryExec follows the group header", launcher->find("[Desktop Entry]\nTryExec=/usr/bin/et\n") == 0);
        check("other keys stay", launcher->find("\nStartupWMClass=et\n") != std::string::npos);

        const auto actions = ngosen::wpsLauncherWithInputMethod(Spreadsheets + "\n[Desktop Action new]\nExec=/usr/bin/et -new\n");
        check("action Exec lines are rewritten too", actions && actions->find("\nExec=env QT_IM_MODULE=fcitx /usr/bin/et -new\n") != std::string::npos);

        const auto own = ngosen::wpsLauncherWithInputMethod("[Desktop Entry]\nTryExec=et\nExec=et\n");
        check("an existing TryExec is kept alone", own && own->find("TryExec=") == own->rfind("TryExec="));
        check("a launcher without Exec is left", !ngosen::wpsLauncherWithInputMethod("[Desktop Entry]\nName=WPS\n"));
    }

    void testDirectories(const std::filesystem::path& root) {
        const auto local   = root / "local/applications";
        const auto system  = root / "usr/applications";
        const auto user    = root / "home/applications";
        const auto missing = root / "missing/applications";
        write(local / "wps-office-et.desktop", "[Desktop Entry]\nExec=/opt/et %F\n");
        write(system / "wps-office-et.desktop", Spreadsheets);
        write(system / "wps-office-wpp.desktop", "[Desktop Entry]\nExec=/usr/bin/wpp %F\n");
        write(system / "wps-office-prometheus.desktop", "[Desktop Entry]\nExec=/usr/bin/wps %U\n");
        write(system / "libreoffice-calc.desktop", "[Desktop Entry]\nExec=libreoffice --calc\n");
        write(user / "wps-office-wpp.desktop", "[Desktop Entry]\nExec=my-wpp\n");

        const auto written = ngosen::addWpsLaunchers({missing, local, system}, user);
        check("two launchers are written", written.size() == 2);
        check("the earlier directory wins", read(user / "wps-office-et.desktop").find("Exec=env QT_IM_MODULE=fcitx /opt/et %F") != std::string::npos);
        check("Writer gets one", read(user / "wps-office-prometheus.desktop").find("Exec=env QT_IM_MODULE=fcitx /usr/bin/wps %U") != std::string::npos);
        check("the user's own launcher stays", read(user / "wps-office-wpp.desktop") == "[Desktop Entry]\nExec=my-wpp\n");
        check("other apps are left", !std::filesystem::exists(user / "libreoffice-calc.desktop"));
        check("a second pass writes nothing", ngosen::addWpsLaunchers({local, system}, user).empty());
        check("no temporary file is left", !std::filesystem::exists(user / "wps-office-et.desktop.ngosen-tmp"));
    }

    void testFlatpak() {
        check("a new override file gets the group", ngosen::flatpakOverrideWithInputMethod("") == std::optional<std::string>("[Environment]\nQT_IM_MODULE=fcitx\n"));
        check("an Environment group gets the key",
              ngosen::flatpakOverrideWithInputMethod("[Context]\nsockets=x11;\n\n[Environment]\nLANG=vi_VN.UTF-8\n") ==
                  std::optional<std::string>("[Context]\nsockets=x11;\n\n[Environment]\nQT_IM_MODULE=fcitx\nLANG=vi_VN.UTF-8\n"));
        check("other groups get an Environment group",
              ngosen::flatpakOverrideWithInputMethod("[Context]\nsockets=x11;") == std::optional<std::string>("[Context]\nsockets=x11;\n\n[Environment]\nQT_IM_MODULE=fcitx\n"));
        check("a module the user set stays", !ngosen::flatpakOverrideWithInputMethod("[Environment]\nQT_IM_MODULE=ibus\n"));
    }

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("ngosen-wps-launcher-" + std::to_string(getpid()));
    std::filesystem::remove_all(root);
    testSession();
    testLauncher();
    testDirectories(root);
    testFlatpak();
    std::filesystem::remove_all(root);
    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "wps launcher: all checks passed\n";
    return EXIT_SUCCESS;
}
