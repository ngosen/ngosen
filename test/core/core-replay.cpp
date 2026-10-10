/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Types the keys of a saved typing log into a fake app, field by field, and compares the app's text
// with the Telex output of the same keys. Only Gõ Sen fields are replayed.
//
// Usage: core_replay typing-XXXX.log
// NGOSEN_REPLAY_LAG_MS, NGOSEN_REPLAY_REPORT_MS and NGOSEN_REPLAY_STALE=1 give the app quirks.

#include "fake-app.h"
#include "ngosen-keysym.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using ngosen::Field;
using ngosen::test::AppQuirks;

namespace {

    // Shift is left out: the key already says which case it types.
    constexpr uint32_t ShortcutStates = ~1U;

    struct Segment {
        Field                 field;
        bool                  sen = false;
        std::vector<uint32_t> keys;
        std::vector<double>   timesMs;
        int                   skipped = 0;
    };

    std::string value(const std::string& detail, const std::string& name) {
        const auto at = detail.find(name + "=");
        if (at == std::string::npos)
            return {};
        const auto from = at + name.size() + 1;
        return detail.substr(from, detail.find(' ', from) - from);
    }

    std::vector<Segment> readLog(std::istream& in) {
        std::vector<Segment> segments;
        for (std::string line; std::getline(in, line);) {
            if (line.empty() || line[0] == '#')
                continue;
            std::istringstream parts(line);
            std::string        time, kind, detail;
            std::getline(parts, time, '\t');
            std::getline(parts, kind, '\t');
            std::getline(parts, detail);
            if (kind == "field") {
                Segment s;
                s.field.frontend        = value(detail, "frontend");
                s.field.program         = value(detail, "program");
                s.field.surroundingText = value(detail, "surrounding") == "1";
                s.field.preedit         = value(detail, "preedit") == "1";
                s.sen                   = value(detail, "mode") == "Sen";
                segments.push_back(s);
            } else if (kind == "key" && !segments.empty() && detail.compare(0, 5, "down ") == 0) {
                Segment&           s      = segments.back();
                const uint32_t     sym    = static_cast<uint32_t>(std::strtoul(detail.c_str() + 5, nullptr, 16));
                const unsigned int states = static_cast<unsigned int>(std::strtoul(value(detail, "states").c_str(), nullptr, 10));
                if ((states & ShortcutStates) != 0) {
                    ++s.skipped;
                    continue;
                }
                s.keys.push_back(sym);
                s.timesMs.push_back(std::strtod(time.c_str(), nullptr));
            }
        }
        return segments;
    }

    std::string describe(const std::vector<uint32_t>& keys) {
        std::string out;
        for (uint32_t k : keys)
            out += k == ngosen::key::BackSpace ? std::string("<BS>") : k >= 0x20 && k < 0x7f ? std::string(1, static_cast<char>(k)) : "<" + std::to_string(k) + ">";
        return out;
    }

    int envInt(const char* name) {
        const char* v = std::getenv(name);
        return v != nullptr ? std::atoi(v) : 0;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s typing-log\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    AppQuirks quirks;
    quirks.lagMs         = envInt("NGOSEN_REPLAY_LAG_MS");
    quirks.reportDelayMs = envInt("NGOSEN_REPLAY_REPORT_MS");
    quirks.staleReport   = envInt("NGOSEN_REPLAY_STALE") != 0;

    int differences = 0;
    for (const Segment& s : readLog(in)) {
        if (!s.sen || s.keys.empty())
            continue;
        std::vector<int> gaps;
        for (size_t i = 0; i < s.keys.size(); ++i)
            gaps.push_back(i + 1 < s.keys.size() ? static_cast<int>(s.timesMs[i + 1] - s.timesMs[i]) : 0);
        const std::string want = ngosen::test::telexOutput(s.keys);
        const std::string seen = ngosen::test::typeInApp(s.field, quirks, s.keys, gaps);
        std::printf("%s %s, keys \"%s\"%s\n  app:   \"%s\"\n  telex: \"%s\"\n", s.field.frontend.c_str(), s.field.program.c_str(), describe(s.keys).c_str(),
                    s.skipped > 0 ? (", left out shortcut keys: " + std::to_string(s.skipped)).c_str() : "", seen.c_str(), want.c_str());
        if (seen != want)
            ++differences;
    }
    return differences == 0 ? 0 : 1;
}
