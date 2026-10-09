/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-ibus-config.h"

#include <cstdlib>
#include <fstream>
#include <map>

namespace ngosen {

    namespace {

        // The defaults of src/lotus-config.h.
        Options defaultOptions() {
            Options o;
            o.inputMethod                      = "Telex";
            o.outputCharset                    = "Unicode";
            o.spellCheck                       = true;
            o.modernStyle                      = true;
            o.freeMarking                      = true;
            o.w2u                              = 1;
            o.timeFormat                       = "%H:%M";
            o.dateFormat                       = "%d/%m/%Y";
            o.autoNonVnRestore                 = true;
            o.ddFreeStyle                      = true;
            o.messengerSelectOvertype          = true;
            o.waitSurroundingEvent             = true;
            o.waitSurroundingMinPerKeyMs       = 8;
            o.waitSurroundingTimeoutMs         = 50;
            o.waitSurroundingShortMs           = 40;
            o.waitSurroundingSettleMs          = 40;
            o.waitSurroundingSettleFirstWordMs = 60;
            o.waitSurroundingProbeEvery        = 4;
            o.surrDeleteSleepMs                = 4;
            o.surrCommitSleepMs                = 3;
            return o;
        }

        std::string trim(const std::string& s) {
            const auto first = s.find_first_not_of(" \t\r");
            if (first == std::string::npos)
                return {};
            const auto last = s.find_last_not_of(" \t\r");
            return s.substr(first, last - first + 1);
        }

        // Top-level Key=Value lines; fcitx5 quotes values that contain spaces.
        std::map<std::string, std::string> readTopLevel(const std::string& path) {
            std::map<std::string, std::string> values;
            std::ifstream                      in(path);
            std::string                        line;
            while (std::getline(in, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                if (line[0] == '[')
                    break;
                const auto eq = line.find('=');
                if (eq == std::string::npos)
                    continue;
                std::string value = trim(line.substr(eq + 1));
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                    value = value.substr(1, value.size() - 2);
                values[trim(line.substr(0, eq))] = value;
            }
            return values;
        }

        void readBool(const std::map<std::string, std::string>& values, const char* key, bool& out) {
            auto it = values.find(key);
            if (it != values.end())
                out = it->second == "True";
        }

        void readInt(const std::map<std::string, std::string>& values, const char* key, int& out) {
            auto it = values.find(key);
            if (it == values.end())
                return;
            char*      end    = nullptr;
            const long parsed = std::strtol(it->second.c_str(), &end, 10);
            if (end != it->second.c_str() && *end == '\0')
                out = static_cast<int>(parsed);
        }

        void readString(const std::map<std::string, std::string>& values, const char* key, std::string& out) {
            auto it = values.find(key);
            if (it != values.end() && !it->second.empty())
                out = it->second;
        }

    } // namespace

    IBusSettings readSettings(const std::string& path) {
        IBusSettings settings;
        settings.options  = defaultOptions();
        const auto values = readTopLevel(path);
        auto&      o      = settings.options;

        readString(values, "InputMethod", o.inputMethod);
        readString(values, "OutputCharset", o.outputCharset);
        readBool(values, "SpellCheck", o.spellCheck);
        readBool(values, "ModernStyle", o.modernStyle);
        readBool(values, "FreeMarking", o.freeMarking);
        readBool(values, "AutoNonVnRestore", o.autoNonVnRestore);
        readBool(values, "DdFreeStyle", o.ddFreeStyle);
        readBool(values, "WaitSurroundingEvent", o.waitSurroundingEvent);
        readInt(values, "WaitSurroundingMinPerKeyMs", o.waitSurroundingMinPerKeyMs);
        readInt(values, "WaitSurroundingTimeoutMs", o.waitSurroundingTimeoutMs);
        readInt(values, "WaitSurroundingShortMs", o.waitSurroundingShortMs);
        readInt(values, "WaitSurroundingSettleMs", o.waitSurroundingSettleMs);
        readInt(values, "WaitSurroundingSettleFirstWordMs", o.waitSurroundingSettleFirstWordMs);
        readInt(values, "WaitSurroundingProbeEvery", o.waitSurroundingProbeEvery);
        readInt(values, "SurrDeleteSleepMs", o.surrDeleteSleepMs);
        readInt(values, "SurrCommitSleepMs", o.surrCommitSleepMs);

        // No menu or emoji list yet, so only these two modes.
        auto mode = values.find("Mode");
        if (mode != values.end() && mode->second == "Preedit")
            settings.mode = Mode::Preedit;
        return settings;
    }

    std::string settingsPath() {
        const char* xdg  = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        std::string base = xdg != nullptr && *xdg != '\0' ? xdg : std::string(home != nullptr ? home : "") + "/.config";
        return base + "/fcitx5/conf/lotus.conf";
    }

} // namespace ngosen
