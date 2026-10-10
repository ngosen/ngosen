/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Random typing in Gõ Sen mode against fake apps that report late, report stale text or apply
// backspaces late. Whatever the app does, its text must end up as the Telex output of the keys.
//
// Keys come 40 to 160 ms apart, with one in five rolled over 8 to 25 ms after the last.
// NGOSEN_FUZZ_RUNS sets the sequences per app (default 2); NGOSEN_FUZZ_SEED repeats one run.

#include "fake-app.h"
#include "ngosen-keysym.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

using ngosen::Field;
using ngosen::test::AppQuirks;
using ngosen::test::telexOutput;
using ngosen::test::typeInApp;

namespace {

    struct App {
        const char* name;
        const char* frontend;
        bool        surroundingText;
        AppQuirks   quirks;
    };

    const App apps[] = {
        {"reports at once", "wayland", true, {}},
        {"reports 20 ms late", "wayland", true, {true, 20, false, 0}},
        {"reports after the wait gives up", "wayland", true, {true, 80, false, 0}},
        {"never reports", "wayland", true, {false, 0, false, 0}},
        {"reports one change behind", "wayland", true, {true, 0, true, 0}},
        {"slow app", "wayland", true, {true, 0, false, 30}},
        {"no surrounding text", "xim", false, {}},
        {"no surrounding text, slow app", "xim", false, {true, 0, false, 30}},
    };

    const char* const     words[] = {"vieetj", "nguowif", "dduwowcj", "tooi", "khoong", "hoaf", "thuyeets", "ddi", "chuaw", "quas", "giof", "muwaf", "xin", "chaof", "banj"};

    std::vector<uint32_t> randomKeys(std::mt19937& rng) {
        std::vector<uint32_t> keys;
        const char            letters[] = "abcdeghiklmnopqrstuvxyfsrxjwd";
        const int             count     = 2 + static_cast<int>(rng() % 5);
        for (int w = 0; w < count; ++w) {
            if (rng() % 3 == 0) {
                const int length = 1 + static_cast<int>(rng() % 5);
                for (int i = 0; i < length; ++i)
                    keys.push_back(static_cast<uint32_t>(letters[rng() % (sizeof(letters) - 1)]));
            } else {
                for (const char* c = words[rng() % (sizeof(words) / sizeof(words[0]))]; *c; ++c)
                    keys.push_back(static_cast<uint32_t>(*c));
            }
            if (rng() % 6 == 0)
                keys.push_back(ngosen::key::BackSpace);
            keys.push_back(ngosen::key::space);
        }
        return keys;
    }

    std::string describe(const std::vector<uint32_t>& keys) {
        std::string out;
        for (uint32_t k : keys)
            out += k == ngosen::key::BackSpace ? std::string("<BS>") : std::string(1, static_cast<char>(k));
        return out;
    }

    unsigned long envNumber(const char* name, unsigned long fallback) {
        const char* value = std::getenv(name);
        return value != nullptr && *value != '\0' ? std::strtoul(value, nullptr, 10) : fallback;
    }

} // namespace

int main() {
    const unsigned long runs     = envNumber("NGOSEN_FUZZ_RUNS", 2);
    const unsigned long seedBase = envNumber("NGOSEN_FUZZ_SEED", 1);
    const bool          oneSeed  = std::getenv("NGOSEN_FUZZ_SEED") != nullptr;
    int                 failures = 0;
    for (const App& app : apps) {
        for (unsigned long run = 0; run < (oneSeed ? 1 : runs); ++run) {
            const unsigned long seed = seedBase + run;
            std::mt19937        rng(static_cast<std::mt19937::result_type>(seed));
            const auto          keys = randomKeys(rng);
            std::vector<int>    gaps;
            for (size_t i = 0; i < keys.size(); ++i)
                gaps.push_back(static_cast<int>(rng() % 5 == 0 ? 8 + rng() % 18 : 40 + rng() % 120));
            const std::string want = telexOutput(keys);
            Field             field;
            field.frontend         = app.frontend;
            field.surroundingText  = app.surroundingText;
            field.preedit          = true;
            const std::string seen = typeInApp(field, app.quirks, keys, gaps);
            if (seen != want) {
                ++failures;
                std::printf("FAIL: %s, seed %lu: keys \"%s\" gave \"%s\", want \"%s\"\n", app.name, seed, describe(keys).c_str(), seen.c_str(), want.c_str());
            }
        }
        std::printf("done: %s\n", app.name);
    }
    return failures == 0 ? 0 : 1;
}
