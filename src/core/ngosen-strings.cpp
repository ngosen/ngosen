/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-strings.h"

#include "ngosen-utf8.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

bool isStartsWith(const std::string& str, const std::string& prefix) {
#if __cplusplus >= 202002L
    return str.starts_with(prefix);
#else
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
#endif
}

bool isEndsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isInHomeDirectory(const std::string& path) {
    return isStartsWith(path, "/home/");
}

std::string stripDesktopSuffix(const std::string& program) {
    static constexpr std::string_view suffix = ".desktop";
    if (program.size() > suffix.size() && program.compare(program.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return program.substr(0, program.size() - suffix.size());
    }
    return program;
}

bool isWordChar(char32_t c) {
    if (c < 0x80)
        return std::isalnum(static_cast<int>(c)) != 0;
    return (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7) || (c >= 0x1E00 && c <= 0x1EFF);
}

std::string withoutToneMark(const std::string& word) {
    // Each row is a vowel followed by its sắc, huyền, hỏi, ngã and nặng forms.
    static const char* const rows[] = {
        "aáàảãạ", "ăắằẳẵặ", "âấầẩẫậ", "eéèẻẽẹ", "êếềểễệ", "iíìỉĩị", "oóòỏõọ", "ôốồổỗộ", "ơớờởỡợ", "uúùủũụ", "ưứừửữự", "yýỳỷỹỵ",
        "AÁÀẢÃẠ", "ĂẮẰẲẴẶ", "ÂẤẦẨẪẬ", "EÉÈẺẼẸ", "ÊẾỀỂỄỆ", "IÍÌỈĨỊ", "OÓÒỎÕỌ", "ÔỐỒỔỖỘ", "ƠỚỜỞỠỢ", "UÚÙỦŨỤ", "ƯỨỪỬỮỰ", "YÝỲỶỸỴ",
    };
    // Decode one code point without allocating; malformed bytes fall back to one raw byte.
    const auto decodeOne = [](const char* data, size_t avail, size_t& bytes) {
        const auto lead = static_cast<unsigned char>(data[0]);
        size_t     want = ngosen::utf8::charBytes(std::string_view(data, avail));
        if (want == 0 || want > avail) {
            bytes = 1;
            return static_cast<uint32_t>(lead);
        }
        uint32_t point = 0;
        if (want == 1) {
            point = lead;
        } else {
            point = static_cast<uint32_t>(lead & (0x7fU >> want));
            for (size_t i = 1; i < want; ++i)
                point = (point << 6) | (static_cast<unsigned char>(data[i]) & 0x3fU);
        }
        bytes = want;
        return point;
    };
    static const auto toneless = [&] {
        std::vector<std::pair<uint32_t, uint32_t>> table;
        for (const char* row : rows) {
            const std::string_view view      = row;
            size_t                 baseBytes = 0;
            const uint32_t         base      = decodeOne(view.data(), view.size(), baseBytes);
            for (size_t i = baseBytes; i < view.size();) {
                size_t         partBytes = 0;
                const uint32_t part      = decodeOne(view.data() + i, view.size() - i, partBytes);
                table.emplace_back(part, base);
                i += partBytes;
            }
        }
        std::sort(table.begin(), table.end());
        return table;
    }();

    std::string result;
    result.reserve(word.size());
    for (size_t i = 0; i < word.size();) {
        size_t         bytes = 0;
        const uint32_t point = decodeOne(word.data() + i, word.size() - i, bytes);
        const auto     found = std::lower_bound(toneless.begin(), toneless.end(), std::make_pair(point, 0U));
        uint32_t       plain = (found != toneless.end() && found->first == point) ? found->second : point;
        if (plain < 0x80) {
            result.push_back(static_cast<char>(plain));
        } else if (plain < 0x800) {
            result.push_back(static_cast<char>(0xc0U | (plain >> 6)));
            result.push_back(static_cast<char>(0x80U | (plain & 0x3fU)));
        } else {
            result.push_back(static_cast<char>(0xe0U | (plain >> 12)));
            result.push_back(static_cast<char>(0x80U | ((plain >> 6) & 0x3fU)));
            result.push_back(static_cast<char>(0x80U | (plain & 0x3fU)));
        }
        i += bytes;
    }
    return result;
}
