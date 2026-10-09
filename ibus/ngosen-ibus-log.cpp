/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-log.h"

#include <cstdlib>
#include <cstring>
#include <glib.h>

namespace ngosen {

    // NGOSEN_DEBUG=1 in the engine's environment turns on the debug and info lines.
    bool logEnabled(LogLevel level) {
        static const bool verbose = std::getenv("NGOSEN_DEBUG") != nullptr;
        return verbose || level == LogLevel::Warn || level == LogLevel::Error;
    }

    void writeLog(LogLevel level, const char* file, int line, const std::string& message) {
        const char*    slash = std::strrchr(file, '/');
        GLogLevelFlags flags = G_LOG_LEVEL_MESSAGE;
        if (level == LogLevel::Warn)
            flags = G_LOG_LEVEL_WARNING;
        else if (level == LogLevel::Error)
            flags = G_LOG_LEVEL_CRITICAL;
        g_log("ngosen", flags, "%s:%d %s", slash != nullptr ? slash + 1 : file, line, message.c_str());
    }

} // namespace ngosen
