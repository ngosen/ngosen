/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "ngosen-menu-icon.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

    int  failures = 0;

    void expectEqual(const std::string& actual, const std::string& expected, const char* what) {
        if (actual != expected) {
            std::cerr << "FAIL " << what << ": got \"" << actual << "\", want \"" << expected << "\"\n";
            ++failures;
        }
    }

    void touch(const std::filesystem::path& path, const std::string& text = "") {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << text;
    }

} // namespace

int main() {
    const auto root  = std::filesystem::temp_directory_path() / ("ngosen-menu-icon-" + std::to_string(getpid()));
    const auto icons = root / "icons";
    touch(icons / "plain" / "index.theme", "[Icon Theme]\nName=plain\nDirectories=16\n\n[16]\nSize=16\nType=Fixed\n");
    touch(icons / "plain" / "16" / "character-set.svg");
    // Only a shorter name, which the lookup would fall back to.
    touch(icons / "plain" / "16" / "format-text.svg");
    // Set before StandardPath first reads them.
    setenv("XDG_DATA_HOME", (root / "home").c_str(), 1);
    setenv("XDG_DATA_DIRS", root.c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);

    expectEqual(fcitx::menuIconName("plain", "character-set", "ngosen-charset"), "character-set", "theme has the icon");
    expectEqual(fcitx::menuIconName("plain", "format-text-capitalize", "ngosen-auto-capitalize"), "ngosen-auto-capitalize", "only a shorter name");
    expectEqual(fcitx::menuIconName("plain", "draw-uppercase", "ngosen-capitalize-macro"), "ngosen-capitalize-macro", "theme lacks the icon");

    std::filesystem::remove_all(root);
    return failures == 0 ? 0 : 1;
}
