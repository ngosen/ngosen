/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "ngosen-gnome-theme.h"
#include "ngosen-utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace fcitx {

    namespace {

        std::optional<std::string> readFile(const std::string& path) {
            std::ifstream file(path);
            if (!file.is_open())
                return std::nullopt;
            std::stringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }

        // First line of a gsettings query, or "" when the schema is missing.
        std::string gsettingsGet(const char* command) {
            std::string result;
            FILE*       pipe = popen(command, "r");
            if (pipe == nullptr)
                return result;
            char buffer[512];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
                result = trim(buffer);
            pclose(pipe);
            return result;
        }

        std::string stripCssComments(const std::string& css) {
            std::string out;
            out.reserve(css.size());
            for (size_t i = 0; i < css.size(); ++i) {
                if (css[i] == '/' && i + 1 < css.size() && css[i + 1] == '*') {
                    const auto end = css.find("*/", i + 2);
                    if (end == std::string::npos)
                        break;
                    i = end + 1;
                    continue;
                }
                out += css[i];
            }
            return out;
        }

        struct Rgba {
            int    r = 0, g = 0, b = 0;
            double a = 1.0;
        };

        int hexDigit(char c) {
            if (c >= '0' && c <= '9')
                return c - '0';
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            return -1;
        }

        // #rgb, #rrggbb, rgb(), rgba(), and the few names shell themes use.
        std::optional<Rgba> parseCssColor(std::string value) {
            value = trim(value);
            if (const auto bang = value.find('!'); bang != std::string::npos)
                value = trim(value.substr(0, bang));
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

            if (value == "white")
                return Rgba{255, 255, 255, 1.0};
            if (value == "black")
                return Rgba{0, 0, 0, 1.0};
            if (value == "transparent")
                return Rgba{0, 0, 0, 0.0};

            if (!value.empty() && value[0] == '#') {
                const std::string hex = value.substr(1);
                std::vector<int>  d;
                for (char c : hex) {
                    const int v = hexDigit(c);
                    if (v < 0)
                        return std::nullopt;
                    d.push_back(v);
                }
                if (d.size() == 3)
                    return Rgba{d[0] * 17, d[1] * 17, d[2] * 17, 1.0};
                if (d.size() == 6)
                    return Rgba{(d[0] * 16) + d[1], (d[2] * 16) + d[3], (d[4] * 16) + d[5], 1.0};
                return std::nullopt;
            }

            const bool hasAlpha = value.rfind("rgba(", 0) == 0;
            if (!hasAlpha && value.rfind("rgb(", 0) != 0)
                return std::nullopt;
            const auto open  = value.find('(');
            const auto close = value.find(')', open);
            if (close == std::string::npos)
                return std::nullopt;
            const auto parts = splitList(value.substr(open + 1, close - open - 1), ',');
            if (parts.size() != (hasAlpha ? 4U : 3U))
                return std::nullopt;
            Rgba color;
            try {
                std::array<int*, 3> channels{&color.r, &color.g, &color.b};
                for (size_t i = 0; i < 3; ++i) {
                    const int c = std::stoi(trim(parts[i]));
                    if (c < 0 || c > 255)
                        return std::nullopt;
                    *channels[i] = c; // NOLINT
                }
                if (hasAlpha)
                    color.a = std::stod(trim(parts[3]));
            } catch (...) { return std::nullopt; }
            return color;
        }

        // Modes that paint the bar with the shell's own compiled-in stylesheet.
        bool isBuiltinShellMode(const std::string& mode) {
            return mode.empty() || mode == "user";
        }

        int gray(const Rgba& c) {
            return ((c.r * 11) + (c.g * 16) + (c.b * 5)) / 32;
        }

    } // namespace

    GnomeShellThemeInfo gnomeShellThemeInfoFromEnv() {
        GnomeShellThemeInfo info;
        info.home        = envOr("HOME", "");
        info.dataHome    = envOr("XDG_DATA_HOME", joinPath(info.home, ".local/share"));
        info.dataDirs    = splitList(envOr("XDG_DATA_DIRS", "/usr/local/share:/usr/share"), ':');
        info.sessionMode = getEnv("GNOME_SHELL_SESSION_MODE");

        // The User Themes name only counts while the extension is enabled.
        const std::string enabled = gsettingsGet("gsettings get org.gnome.shell enabled-extensions 2>/dev/null");
        const std::string noUser  = gsettingsGet("gsettings get org.gnome.shell disable-user-extensions 2>/dev/null");
        if (enabled.find("'user-theme@gnome-shell-extensions.gcampax.github.com'") != std::string::npos && noUser != "true") {
            std::string name = gsettingsGet("gsettings get org.gnome.shell.extensions.user-theme name 2>/dev/null");
            if (name.size() >= 2 && name.front() == '\'' && name.back() == '\'')
                info.userThemeName = name.substr(1, name.size() - 2);
        }

        if (isBuiltinShellMode(info.sessionMode) && info.userThemeName.empty()) {
            const std::string scheme = gsettingsGet("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null");
            if (scheme.size() >= 2 && scheme.front() == '\'' && scheme.back() == '\'')
                info.colorScheme = scheme.substr(1, scheme.size() - 2);
            // The running shell's version; it only changes with a new login.
            static int version = 0;
            if (version == 0)
                version = gnomeShellMajorVersion(gsettingsGet("busctl --user --timeout=1 get-property org.gnome.Shell /org/gnome/Shell org.gnome.Shell ShellVersion 2>/dev/null"));
            info.shellMajorVersion = version;
        }
        return info;
    }

    int gnomeShellMajorVersion(const std::string& versionLine) {
        const auto digit = versionLine.find_first_of("0123456789");
        if (digit == std::string::npos)
            return 0;
        try {
            return std::stoi(versionLine.substr(digit));
        } catch (...) { return 0; }
    }

    bool isGnomeShellSession(const std::string& xdgCurrentDesktop) {
        const auto desktops = splitList(xdgCurrentDesktop, ':');
        if (std::find(desktops.begin(), desktops.end(), "GNOME") == desktops.end())
            return false;
        static const std::array<const char*, 6> kOwnPanel{"Budgie", "Pantheon", "GNOME-Flashback", "Unity", "X-Cinnamon", "MATE"};
        return std::none_of(desktops.begin(), desktops.end(), [](const std::string& d) { return std::find(kOwnPanel.begin(), kOwnPanel.end(), d) != kOwnPanel.end(); });
    }

    std::optional<bool> isShellCssPanelDark(const std::string& rawCss) {
        const std::string          css = stripCssComments(rawCss);
        std::optional<std::string> text;
        std::optional<std::string> background;

        // Every rule whose whole selector is "#panel"; later ones override.
        size_t pos = 0;
        while ((pos = css.find("#panel", pos)) != std::string::npos) {
            const size_t after          = pos + 6;
            const bool   startsSelector = pos == 0 || css[pos - 1] == '}' || std::isspace(static_cast<unsigned char>(css[pos - 1]));
            size_t       brace          = after;
            while (brace < css.size() && std::isspace(static_cast<unsigned char>(css[brace])))
                ++brace;
            pos = after;
            if (!startsSelector || brace >= css.size() || css[brace] != '{')
                continue;
            if (pos > 6) {
                // Reject "a, #panel {" — the rule then targets more than the bar.
                size_t back = pos - 7;
                while (back > 0 && std::isspace(static_cast<unsigned char>(css[back])))
                    --back;
                if (css[back] == ',')
                    continue;
            }
            const auto end = css.find('}', brace);
            if (end == std::string::npos)
                break;
            for (const auto& decl : splitList(css.substr(brace + 1, end - brace - 1), ';')) {
                const auto colon = decl.find(':');
                if (colon == std::string::npos)
                    continue;
                const std::string prop = trim(decl.substr(0, colon));
                if (prop == "color")
                    text = decl.substr(colon + 1);
                else if (prop == "background-color" || prop == "background")
                    background = decl.substr(colon + 1);
            }
            pos = end;
        }

        if (text) {
            if (const auto c = parseCssColor(*text))
                return gray(*c) >= 128; // light text is drawn on a dark bar
        }
        if (background) {
            // Same threshold as the KDE check (xdg-desktop-portal-kde).
            if (const auto c = parseCssColor(*background); c && c->a >= 0.5)
                return gray(*c) < 192;
        }
        return std::nullopt;
    }

    std::optional<std::string> gnomeShellStylesheet(const GnomeShellThemeInfo& info) {
        // User Themes extension: same search order as its util.js.
        if (!info.userThemeName.empty()) {
            std::vector<std::string> dirs{joinPath(info.home, ".themes"), joinPath(info.dataHome, "themes")};
            for (const auto& dir : info.dataDirs)
                dirs.push_back(joinPath(dir, "themes"));
            for (const auto& dir : dirs) {
                const std::string path = joinPath(dir, info.userThemeName + "/gnome-shell/gnome-shell.css");
                if (fileReadable(path))
                    return path;
            }
            for (const auto& dir : info.dataDirs) {
                const std::string path = joinPath(dir, "gnome-shell/theme/" + info.userThemeName + ".css");
                if (fileReadable(path))
                    return path;
            }
        }

        // Session mode stylesheet, e.g. Ubuntu's "Yaru/gnome-shell.css".
        if (isBuiltinShellMode(info.sessionMode))
            return std::nullopt;
        std::vector<std::string> modeDirs{info.dataHome};
        modeDirs.insert(modeDirs.end(), info.dataDirs.begin(), info.dataDirs.end());
        for (const auto& dir : modeDirs) {
            const auto json = readFile(joinPath(dir, "gnome-shell/modes/" + info.sessionMode + ".json"));
            if (!json)
                continue;
            const auto key = json->find("\"stylesheetName\"");
            if (key == std::string::npos)
                return std::nullopt;
            const auto open  = json->find('"', json->find(':', key));
            const auto close = open == std::string::npos ? std::string::npos : json->find('"', open + 1);
            if (close == std::string::npos)
                return std::nullopt;
            const std::string name = json->substr(open + 1, close - open - 1);
            for (const auto& themeDir : info.dataDirs) {
                const std::string path = joinPath(themeDir, "gnome-shell/theme/" + name);
                if (fileReadable(path))
                    return path;
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<bool> isGnomePanelDark(const GnomeShellThemeInfo& info) {
        const auto path = gnomeShellStylesheet(info);
        if (!path) {
            // Only GNOME 51 and later are known to follow color-scheme here.
            if (isBuiltinShellMode(info.sessionMode) && info.shellMajorVersion >= 51)
                return info.colorScheme != "prefer-light";
            return std::nullopt;
        }
        const auto css = readFile(*path);
        if (!css)
            return std::nullopt;
        return isShellCssPanelDark(*css);
    }

    CinnamonThemeInfo cinnamonThemeInfoFromEnv() {
        CinnamonThemeInfo info;
        info.home        = envOr("HOME", "");
        info.dataHome    = envOr("XDG_DATA_HOME", joinPath(info.home, ".local/share"));
        info.dataDirs    = splitList(envOr("XDG_DATA_DIRS", "/usr/local/share:/usr/share"), ':');
        std::string name = gsettingsGet("gsettings get org.cinnamon.theme name 2>/dev/null");
        if (name.size() >= 2 && name.front() == '\'' && name.back() == '\'')
            info.themeName = name.substr(1, name.size() - 2);
        return info;
    }

    bool isCinnamonSession(const std::string& xdgCurrentDesktop, const std::string& desktopSession) {
        const auto desktops = splitList(xdgCurrentDesktop, ':');
        if (std::find(desktops.begin(), desktops.end(), "X-Cinnamon") != desktops.end())
            return true;
        // "cinnamon", "cinnamon2d" or "cinnamon-wayland"
        return xdgCurrentDesktop.empty() && desktopSession.rfind("cinnamon", 0) == 0;
    }

    bool trayWantsIconName(const std::string& xdgCurrentDesktop, const std::string& desktopSession, const std::string& swaySock) {
        const auto desktops = splitList(xdgCurrentDesktop, ':');
        const auto named    = [&desktops](const char* name) { return std::find(desktops.begin(), desktops.end(), name) != desktops.end(); };
        return isCinnamonSession(xdgCurrentDesktop, desktopSession) || named("COSMIC") || named("sway") || (xdgCurrentDesktop.empty() && !swaySock.empty());
    }

    std::string trayIconName(const std::string& baseIconName, TrayIconStyle style, bool large, bool gnomeShell) {
        switch (style) {
            case TrayIconStyle::PlainWhite: return baseIconName + "-default";
            case TrayIconStyle::PlainBlack: return baseIconName + "-default-black";
            case TrayIconStyle::Tile: break;
        }
        return large && !gnomeShell ? baseIconName + "-large" : baseIconName;
    }

    std::vector<std::string> trayIconNames(const std::string& iconName, const std::string& baseIconName, const std::string& xdgCurrentDesktop) {
        if (isGnomeShellSession(xdgCurrentDesktop))
            return {iconName + "-gnome", iconName, baseIconName};
        return {iconName, baseIconName};
    }

    std::optional<std::string> cinnamonStylesheet(const CinnamonThemeInfo& info) {
        if (info.themeName.empty() || info.themeName == "cinnamon") {
            for (const auto& dir : info.dataDirs) {
                const std::string path = joinPath(dir, "cinnamon/theme/cinnamon.css");
                if (fileReadable(path))
                    return path;
            }
            return std::nullopt;
        }
        // Same search order as Cinnamon's theme manager.
        std::vector<std::string> dirs{joinPath(info.home, ".themes"), joinPath(info.dataHome, "themes")};
        for (const auto& dir : info.dataDirs)
            dirs.push_back(joinPath(dir, "themes"));
        for (const auto& dir : dirs) {
            const std::string path = joinPath(dir, info.themeName + "/cinnamon/cinnamon.css");
            if (fileReadable(path))
                return path;
        }
        return std::nullopt;
    }

    std::optional<bool> isCinnamonPanelDark(const CinnamonThemeInfo& info) {
        const auto path = cinnamonStylesheet(info);
        if (!path)
            return std::nullopt;
        const auto css = readFile(*path);
        if (!css)
            return std::nullopt;
        return isShellCssPanelDark(*css);
    }

    bool isDeepinSession(const std::string& xdgCurrentDesktop) {
        const auto desktops = splitList(xdgCurrentDesktop, ':');
        return std::find(desktops.begin(), desktops.end(), "DDE") != desktops.end();
    }

    std::optional<bool> isDeepinPanelDark(const std::string& globalTheme) {
        const auto dot = globalTheme.rfind('.');
        if (dot == std::string::npos)
            return std::nullopt;
        const std::string variant = globalTheme.substr(dot + 1);
        if (variant == "dark")
            return true;
        if (variant == "light")
            return false;
        return std::nullopt;
    }

    std::string deepinGlobalThemeFromSession() {
        return busctlStringValue(gsettingsGet("busctl --user --timeout=1 get-property org.deepin.dde.Appearance1 /org/deepin/dde/Appearance1 "
                                              "org.deepin.dde.Appearance1 GlobalTheme 2>/dev/null"));
    }

    std::string busctlStringValue(const std::string& reply) {
        if (reply.rfind("s \"", 0) != 0 || reply.size() < 4 || reply.back() != '"')
            return {};
        return reply.substr(3, reply.size() - 4);
    }

} // namespace fcitx
