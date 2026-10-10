/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-utils.h"
#include "lotus-config.h"

#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <fcitx-utils/utf8.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

FCITX_DEFINE_LOG_CATEGORY(lotus, "ngosen", fcitx::LogLevel::NoLog);

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool isUinputMode(fcitx::LotusMode mode) {
    return mode == fcitx::LotusMode::Sen;
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

std::string getEnv(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return ((value != nullptr) && ((*value) != 0)) ? value : "";
}