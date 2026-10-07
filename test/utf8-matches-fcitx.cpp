// SPDX-License-Identifier: GPL-3.0-or-later
//
// The typing logic's UTF-8 helpers must answer exactly as the fcitx5 ones they replace, broken
// input included, so compare them on every short byte string and on random longer ones.
// fcitx5 before 5.1.12 also rejected noncharacters such as U+FFFF; the helpers follow later fcitx5,
// so strings holding one are compared only where both versions agree.
#include "ngosen-utf8.h"

#include <fcitx-utils/utf8.h>

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

    int         failures = 0;

    std::string hex(const std::string& s) {
        static const char* digits = "0123456789abcdef";
        std::string        out;
        for (unsigned char c : s) {
            out += digits[c >> 4];
            out += digits[c & 0xf];
            out += ' ';
        }
        return out;
    }

    void fail(const std::string& what, const std::string& s) {
        if (++failures <= 20) {
            std::cerr << "FAIL: " << what << " on [" << hex(s) << "]\n";
        }
    }

    bool fcitxDecode(const std::string& s, std::u32string& out) {
        try {
            auto range = fcitx::utf8::MakeUTF8CharRange(s);
            out.assign(range.begin(), range.end());
            return true;
        } catch (const std::runtime_error&) { return false; }
    }

    bool ownDecode(const std::string& s, std::u32string& out) {
        try {
            out = ngosen::utf8::decode(s);
            return true;
        } catch (const std::runtime_error&) { return false; }
    }

    bool isNoncharacter(char32_t c) {
        return (c >= 0xfdd0 && c <= 0xfdef) || (c & 0xfffe) == 0xfffe;
    }

    // Decodes here rather than with the helpers under test, so a bug in them cannot hide a string.
    bool hasNoncharacter(const std::string& s) {
        auto at   = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
        auto cont = [&](size_t i) { return i < s.size() && (at(i) & 0xc0) == 0x80; };
        for (size_t i = 0; i < s.size(); ++i) {
            if ((at(i) & 0xf0) == 0xe0 && cont(i + 1) && cont(i + 2)) {
                if (isNoncharacter(((at(i) & 0x0fU) << 12) | ((at(i + 1) & 0x3fU) << 6) | (at(i + 2) & 0x3fU))) {
                    return true;
                }
            } else if ((at(i) & 0xf8) == 0xf0 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
                if (isNoncharacter(((at(i) & 0x07U) << 18) | ((at(i + 1) & 0x3fU) << 12) | ((at(i + 2) & 0x3fU) << 6) | (at(i + 3) & 0x3fU))) {
                    return true;
                }
            }
        }
        return false;
    }

    void compare(const std::string& s) {
        if (ngosen::utf8::length(s) != fcitx::utf8::length(s)) {
            fail("length", s);
        }
        const bool versionsDiffer = hasNoncharacter(s);
        if (!versionsDiffer && ngosen::utf8::validate(s) != fcitx::utf8::validate(s)) {
            fail("validate", s);
        }
        for (auto it = s.begin(); it != s.end(); ++it) {
            if (ngosen::utf8::nextChar(it, s.end()) != fcitx::utf8::nextChar(it)) {
                fail("nextChar at byte " + std::to_string(it - s.begin()), s);
            }
        }
        std::u32string theirs;
        std::u32string ours;
        const bool     theyDecode = fcitxDecode(s, theirs);
        if (!versionsDiffer && (ownDecode(s, ours) != theyDecode || (theyDecode && ours != theirs))) {
            fail("decode", s);
        }
    }

} // namespace

int main() {
    // Lead bytes of every length, continuation bytes at the edges, overlong and surrogate starts,
    // bytes that never appear in UTF-8, and NUL.
    const std::vector<unsigned char> bytes = {0x00, 0x41, 0x7f, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xa1, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xdf,
                                              0xe0, 0xe1, 0xed, 0xee, 0xef, 0xf0, 0xf4, 0xf5, 0xf7, 0xf8, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
    std::string                      s;
    size_t                           compared = 0;
    // Every string of up to four of these bytes.
    std::vector<std::string> level = {""};
    for (int n = 0; n <= 4; ++n) {
        std::vector<std::string> longer;
        for (const auto& t : level) {
            compare(t);
            ++compared;
            if (n < 4) {
                for (unsigned char b : bytes) {
                    longer.push_back(t + char(b));
                }
            }
        }
        level = std::move(longer);
    }

    std::mt19937                       rng(20261007);
    std::uniform_int_distribution<int> pick(0, static_cast<int>(bytes.size()) - 1);
    std::uniform_int_distribution<int> any(0, 255);
    std::uniform_int_distribution<int> len(0, 16);
    for (int i = 0; i < 200000; ++i) {
        s.clear();
        for (int n = len(rng); n > 0; --n) {
            s += char(i % 2 ? bytes[pick(rng)] : any(rng));
        }
        compare(s);
        ++compared;
    }

    for (const std::string& text : {std::string("Tiếng Việt"), std::string("người\nđi"), std::string("😀 ok"), std::string("a\0b", 3), std::string("đ\xff")}) {
        compare(text);
        ++compared;
    }

    if (!ngosen::utf8::validate("\xef\xbf\xbf") || ngosen::utf8::decode("\xf4\x8f\xbf\xbf") != U"\U0010ffff") {
        fail("noncharacters are accepted", "\xef\xbf\xbf");
    }

    std::cerr << compared << " strings compared, " << failures << " differences\n";
    return failures == 0 ? 0 : 1;
}
