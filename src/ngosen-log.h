/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <sstream>
#include <string>

namespace ngosen {

    enum class LogLevel {
        Debug,
        Info,
        Warn,
        Error,
    };

    // Defined by the input method side, which decides where log lines go and which levels are on.
    bool logEnabled(LogLevel level);
    void writeLog(LogLevel level, const char* file, int line, const std::string& message);

} // namespace ngosen

// The message is built only when its level is on.
#define NGOSEN_LOG(level, prefix, msg)                                                                                                                                             \
    do {                                                                                                                                                                           \
        if (::ngosen::logEnabled(level)) {                                                                                                                                         \
            std::ostringstream ngosenLogStream_;                                                                                                                                   \
            ngosenLogStream_ << prefix << msg;                                                                                                                                     \
            ::ngosen::writeLog(level, __FILE__, __LINE__, ngosenLogStream_.str());                                                                                                 \
        }                                                                                                                                                                          \
    } while (false)

#define NGOSEN_DEBUG(msg) NGOSEN_LOG(::ngosen::LogLevel::Debug, "[DEBUG] ", msg)
#define NGOSEN_INFO(msg)  NGOSEN_LOG(::ngosen::LogLevel::Info, "[INFO] ", msg)
#define NGOSEN_WARN(msg)  NGOSEN_LOG(::ngosen::LogLevel::Warn, "[WARN] ", msg)
#define NGOSEN_ERROR(msg) NGOSEN_LOG(::ngosen::LogLevel::Error, "[ERROR] ", msg)
