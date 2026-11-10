/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

namespace ngosen {

    // What a report after a replacement through the surrounding text says the app did with it.
    enum class SurroundingVerdict {
        DroppedDeletion, // the old part is still there, with the new one after it
        Correct,
        WrongEarlierPart, // the new part is there, but what stood before it changed
        NoAnswer,         // the report does not show the new part yet
    };

    // beforeCursor is the reported text up to the cursor; prefix + deleted was the word before the
    // replacement and prefix + added after it. keysAfter is how many keys were typed since.
    SurroundingVerdict judgeSurroundingReport(const std::string& beforeCursor, const std::string& prefix, const std::string& deleted, const std::string& added,
                                              unsigned int keysAfter);

    const char*        verdictName(SurroundingVerdict verdict);

} // namespace ngosen
