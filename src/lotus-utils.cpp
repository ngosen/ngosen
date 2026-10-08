/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-utils.h"
#include "lotus-config.h"
#include "ngosen-keysym.h"

#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <fcitx-utils/utf8.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

// Global variables
std::atomic<fcitx::LotusMode> realMode{fcitx::LotusMode::Sen};
std::atomic<bool>             needEngineReset{false};
std::atomic<bool>             g_mouse_clicked{false};
std::atomic<bool>             is_deleting_{false};
std::atomic<bool>             stop_flag_monitor{false};
std::atomic<unsigned int>     realtextLen{0};

FCITX_DEFINE_LOG_CATEGORY(lotus, "lotus", fcitx::LogLevel::NoLog);

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool isBackspace(uint32_t sym) {
    return sym == 65288 || sym == 8 || sym == ngosen::key::BackSpace;
}

bool isUinputMode(fcitx::LotusMode mode) {
    return mode == fcitx::LotusMode::Sen;
}

int compareAndSplitStrings(const std::string& A, const std::string& B, std::string& deletedPart, std::string& addedPart) {
    size_t i = 0;
    size_t j = 0;
    while (i < A.size() && j < B.size()) {
        unsigned int lenA = fcitx_utf8_char_len(&A[i]);
        unsigned int lenB = fcitx_utf8_char_len(&B[j]);
        if (lenA == 0 || lenB == 0) {
            break;
        }
        if (i + lenA > A.size() || j + lenB > B.size()) {
            break;
        }
        if (lenA == lenB && std::strncmp(&A[i], &B[j], lenA) == 0) {
            i += lenA;
            j += lenB;
        } else {
            break;
        }
    }

    deletedPart.assign(A, i);
    addedPart.assign(B, j);
    return (deletedPart.empty() && addedPart.empty()) ? 1 : 2;
}

bool isStartsWith(const std::string& str, const std::string& prefix) {
#if __cplusplus >= 202002L
    return str.starts_with(prefix);
#else
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
#endif
}

std::string getFrontendName(fcitx::InputContext* ic) {
    if (ic == nullptr) {
        return "unknown";
    }
    return ic->frontend();
}

bool dropStaleSurroundingText(fcitx::InputContext* ic) {
    if (ic == nullptr || ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText) || !ic->surroundingText().isValid()) {
        return false;
    }
    ic->surroundingText().invalidate();
    return true;
}

std::string stripDesktopSuffix(const std::string& program) {
    static constexpr std::string_view suffix = ".desktop";
    if (program.size() > suffix.size() && program.compare(program.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return program.substr(0, program.size() - suffix.size());
    }
    return program;
}

void eraseLastUtf8Codepoint(std::string& buffer) {
    if (buffer.empty()) {
        return;
    }
    size_t pos = buffer.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(buffer[pos]) & 0xC0) == 0x80) {
        --pos;
    }
    buffer.erase(pos);
}

std::string getEnv(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return ((value != nullptr) && ((*value) != 0)) ? value : "";
}