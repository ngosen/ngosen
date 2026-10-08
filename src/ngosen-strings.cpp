/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-strings.h"

#include <string_view>

bool isStartsWith(const std::string& str, const std::string& prefix) {
#if __cplusplus >= 202002L
    return str.starts_with(prefix);
#else
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
#endif
}

std::string stripDesktopSuffix(const std::string& program) {
    static constexpr std::string_view suffix = ".desktop";
    if (program.size() > suffix.size() && program.compare(program.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return program.substr(0, program.size() - suffix.size());
    }
    return program;
}
