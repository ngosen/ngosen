/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-wps-launcher.h"

#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace ngosen {

    namespace {
        const std::string InputMethodEnv = "QT_IM_MODULE=fcitx";

        bool              startsWith(const std::string& text, const std::string& prefix) {
            return text.compare(0, prefix.size(), prefix) == 0;
        }

        bool hasPart(const std::string& list, char separator, const std::string& part) {
            std::istringstream in(list);
            std::string        item;
            while (std::getline(in, item, separator)) {
                if (item == part)
                    return true;
            }
            return false;
        }

        std::optional<std::string> readFile(const std::filesystem::path& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return std::nullopt;
            std::ostringstream text;
            text << in.rdbuf();
            return text.str();
        }

        // Written beside the target and renamed, so a launcher is never seen half written.
        bool writeFile(const std::filesystem::path& path, const std::string& text) {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            const auto temporary = path.string() + ".ngosen-tmp";
            {
                std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                if (!(out << text))
                    return false;
            }
            std::filesystem::rename(temporary, path, ec);
            if (ec)
                std::filesystem::remove(temporary, ec);
            return !ec;
        }
    } // namespace

    bool wantsWpsInputMethod(const std::string& currentDesktop, const std::string& sessionType) {
        return sessionType == "wayland" && hasPart(currentDesktop, ':', "KDE");
    }

    std::optional<std::string> wpsLauncherWithInputMethod(const std::string& entry) {
        std::istringstream in(entry);
        std::ostringstream out;
        std::string        line;
        std::string        program;
        bool               hasTryExec = false;
        while (std::getline(in, line)) {
            if (startsWith(line, "Exec=")) {
                if (program.empty())
                    program = line.substr(5, line.find(' ', 5) - 5);
                line = "Exec=env " + InputMethodEnv + " " + line.substr(5);
            }
            hasTryExec = hasTryExec || startsWith(line, "TryExec=");
            out << line << '\n';
        }
        if (program.empty())
            return std::nullopt;
        std::string text = out.str();
        // Hides the copy once WPS is removed.
        const std::string header = "[Desktop Entry]\n";
        if (const auto at = text.find(header); !hasTryExec && at != std::string::npos)
            text.insert(at + header.size(), "TryExec=" + program + "\n");
        return text;
    }

    std::vector<std::string> addWpsLaunchers(const std::vector<std::filesystem::path>& systemDirs, const std::filesystem::path& userDir) {
        std::vector<std::string> written;
        std::set<std::string>    seen;
        std::error_code          ec;
        for (const auto& dir : systemDirs) {
            for (const auto& file : std::filesystem::directory_iterator(dir, ec)) {
                const auto name = file.path().filename().string();
                if (!startsWith(name, "wps-office-") || file.path().extension() != ".desktop" || !seen.insert(name).second)
                    continue;
                if (std::filesystem::exists(userDir / name, ec))
                    continue;
                const auto entry    = readFile(file.path());
                const auto launcher = entry ? wpsLauncherWithInputMethod(*entry) : std::nullopt;
                if (launcher && writeFile(userDir / name, *launcher))
                    written.push_back(name);
            }
        }
        return written;
    }

    std::optional<std::string> flatpakOverrideWithInputMethod(const std::string& overrides) {
        std::istringstream in(overrides);
        std::string        line;
        while (std::getline(in, line)) {
            if (startsWith(line, "QT_IM_MODULE="))
                return std::nullopt;
        }
        const std::string group = "[Environment]\n";
        std::string       text  = overrides;
        if (const auto at = text.find(group); at != std::string::npos && (at == 0 || text[at - 1] == '\n')) {
            text.insert(at + group.size(), InputMethodEnv + "\n");
            return text;
        }
        if (!text.empty() && text.back() != '\n')
            text += '\n';
        if (!text.empty())
            text += '\n';
        return text + group + InputMethodEnv + "\n";
    }

} // namespace ngosen
