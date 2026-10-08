/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

// UTF-8 helpers for the typing logic. They behave like the fcitx5 ones they replace, including on
// broken input, so moving off fcitx5 changes nothing the user sees.
namespace ngosen::utf8 {

    // Bytes taken by the character at the start of s. A malformed sequence counts as one byte, and a
    // NUL byte as zero: like fcitx5, the text ends there.
    size_t charBytes(std::string_view s);

    // Number of characters in s up to its first NUL byte, counting a malformed byte as one.
    size_t length(std::string_view s);

    // Whether s is valid UTF-8 up to its first NUL byte.
    bool validate(std::string_view s);

    // The characters of s as code points. Throws std::runtime_error on malformed input, which
    // callers rule out by reading only text the app reported as valid.
    std::u32string decode(std::string_view s);

    // Removes the last character of buffer, if any, so it stays valid UTF-8.
    void eraseLastCodepoint(std::string& buffer);

    // Splits a and b after their longest common run of whole characters: deletedPart gets the rest of a,
    // addedPart the rest of b. Returns 1 when the strings are equal, otherwise 2.
    int compareAndSplitStrings(const std::string& a, const std::string& b, std::string& deletedPart, std::string& addedPart);

    // The iterator one character past it.
    template <typename Iter>
    Iter nextChar(Iter it, Iter end) {
        return std::next(it, static_cast<std::ptrdiff_t>(charBytes(std::string_view(&*it, static_cast<size_t>(std::distance(it, end))))));
    }

} // namespace ngosen::utf8
