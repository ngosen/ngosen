/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-log.h"

#include "ngosen-utils.h"

#include <cstring>
#include <fcitx-utils/log.h>

namespace ngosen {

    namespace {
        fcitx::LogLevel toFcitx(LogLevel level) {
            switch (level) {
                case LogLevel::Debug: return fcitx::LogLevel::Debug;
                case LogLevel::Info: return fcitx::LogLevel::Info;
                case LogLevel::Warn: return fcitx::LogLevel::Warn;
                case LogLevel::Error: return fcitx::LogLevel::Error;
            }
            return fcitx::LogLevel::Error;
        }
    } // namespace

    bool logEnabled(LogLevel level) {
        return ngosenLog().checkLogLevel(toFcitx(level));
    }

    // fcitx5 prints only the file's base name.
    void writeLog(LogLevel level, const char* file, int line, const std::string& message) {
        const char* slash = std::strrchr(file, '/');
        fcitx::LogMessageBuilder(fcitx::Log::logStream(), toFcitx(level), slash ? slash + 1 : file, line).self() << message;
    }

} // namespace ngosen
