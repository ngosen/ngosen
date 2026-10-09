/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-log.h"

#include <cstdlib>
#include <cstring>
#include <syslog.h>

namespace ngosen {

    // NGOSEN_DEBUG=1 in the engine's environment turns on the debug and info lines.
    bool logEnabled(LogLevel level) {
        static const bool verbose = std::getenv("NGOSEN_DEBUG") != nullptr;
        return verbose || level == LogLevel::Warn || level == LogLevel::Error;
    }

    // ibus-daemon drops the engine's stderr, so the lines go to the journal.
    void writeLog(LogLevel level, const char* file, int line, const std::string& message) {
        const char* slash    = std::strrchr(file, '/');
        int         priority = LOG_DEBUG;
        if (level == LogLevel::Info)
            priority = LOG_INFO;
        else if (level == LogLevel::Warn)
            priority = LOG_WARNING;
        else if (level == LogLevel::Error)
            priority = LOG_ERR;
        syslog(LOG_USER | priority, "%s:%d %s", slash != nullptr ? slash + 1 : file, line, message.c_str());
    }

} // namespace ngosen
