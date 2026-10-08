/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-utf8.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace ngosen::utf8 {

    namespace {

        bool isContinuation(std::string_view s, size_t i) {
            return i < s.size() && (static_cast<unsigned char>(s[i]) & 0xc0) == 0x80;
        }

        // Length a lead byte announces, or 0 when it cannot start a character. Five- and six-byte forms
        // are kept so a lenient step skips them whole, as fcitx5 does.
        size_t announcedBytes(unsigned char b) {
            if (b < 0x80) {
                return 1;
            }
            if (b < 0xc0) {
                return 0;
            }
            if (b < 0xe0) {
                return 2;
            }
            if (b < 0xf0) {
                return 3;
            }
            if (b < 0xf8) {
                return 4;
            }
            if (b < 0xfc) {
                return 5;
            }
            if (b < 0xfe) {
                return 6;
            }
            return 0;
        }

        // Steps over a well-formed lead plus continuations without checking the value.
        size_t lenientBytes(std::string_view s) {
            const size_t n = announcedBytes(static_cast<unsigned char>(s[0]));
            if (n <= 1) {
                return 1;
            }
            for (size_t i = 1; i < n; ++i) {
                if (!isContinuation(s, i)) {
                    return 1;
                }
            }
            return n;
        }

        size_t encodedBytes(uint32_t c) {
            if (c < 0x80) {
                return 1;
            }
            if (c < 0x800) {
                return 2;
            }
            if (c < 0x10000) {
                return 3;
            }
            if (c < 0x200000) {
                return 4;
            }
            if (c < 0x4000000) {
                return 5;
            }
            return 6;
        }

        // Decodes one character strictly; returns 0 bytes when it is malformed, overlong, a surrogate
        // or beyond U+10FFFF.
        size_t strictChar(std::string_view s, uint32_t& out) {
            const auto   b = static_cast<unsigned char>(s[0]);
            const size_t n = announcedBytes(b);
            if (n == 0 || n > s.size()) {
                return 0;
            }
            if (n == 1) {
                out = b;
                return 1;
            }
            uint32_t c = b & (0x7fU >> n);
            for (size_t i = 1; i < n; ++i) {
                if (!isContinuation(s, i)) {
                    return 0;
                }
                c = (c << 6) | (static_cast<unsigned char>(s[i]) & 0x3fU);
            }
            if (encodedBytes(c) != n || c >= 0x110000 || (c & 0xfffff800U) == 0xd800) {
                return 0;
            }
            out = c;
            return n;
        }

    } // namespace

    size_t charBytes(std::string_view s) {
        if (s.empty() || s[0] == '\0') {
            return 0;
        }
        return lenientBytes(s);
    }

    size_t length(std::string_view s) {
        size_t count = 0;
        while (!s.empty() && s[0] != '\0') {
            s.remove_prefix(lenientBytes(s));
            ++count;
        }
        return count;
    }

    bool validate(std::string_view s) {
        while (!s.empty() && s[0] != '\0') {
            uint32_t     c = 0;
            const size_t n = strictChar(s, c);
            if (n == 0) {
                return false;
            }
            s.remove_prefix(n);
        }
        return true;
    }

    std::u32string decode(std::string_view s) {
        std::u32string out;
        while (!s.empty()) {
            uint32_t     c = 0;
            const size_t n = strictChar(s, c);
            if (n == 0) {
                throw std::runtime_error("Invalid UTF8 character.");
            }
            out.push_back(static_cast<char32_t>(c));
            s.remove_prefix(n);
        }
        return out;
    }

    void eraseLastCodepoint(std::string& buffer) {
        if (buffer.empty()) {
            return;
        }
        size_t pos = buffer.size() - 1;
        while (pos > 0 && (static_cast<unsigned char>(buffer[pos]) & 0xC0) == 0x80) {
            --pos;
        }
        buffer.erase(pos);
    }

    int compareAndSplitStrings(const std::string& a, const std::string& b, std::string& deletedPart, std::string& addedPart) {
        // A byte that cannot start a character counts as one, as in fcitx5's fcitx_utf8_char_len.
        const auto leadBytes = [](char c) {
            const size_t n = announcedBytes(static_cast<unsigned char>(c));
            return n == 0 ? size_t{1} : n;
        };
        size_t i = 0;
        size_t j = 0;
        while (i < a.size() && j < b.size()) {
            const size_t lenA = leadBytes(a[i]);
            const size_t lenB = leadBytes(b[j]);
            if (i + lenA > a.size() || j + lenB > b.size()) {
                break;
            }
            if (lenA == lenB && std::strncmp(&a[i], &b[j], lenA) == 0) {
                i += lenA;
                j += lenB;
            } else {
                break;
            }
        }

        deletedPart.assign(a, i);
        addedPart.assign(b, j);
        return (deletedPart.empty() && addedPart.empty()) ? 1 : 2;
    }

} // namespace ngosen::utf8
