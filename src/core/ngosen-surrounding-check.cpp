/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-surrounding-check.h"

#include "ngosen-strings.h"

namespace ngosen {

    SurroundingVerdict judgeSurroundingReport(const std::string& beforeCursor, const std::string& prefix, const std::string& deleted, const std::string& added,
                                              unsigned int keysAfter) {
        // The report may show any of the keys typed since, so drop up to that many characters from its end.
        size_t end = beforeCursor.size();
        for (unsigned int dropped = 0; dropped <= keysAfter; ++dropped) {
            const std::string shown = beforeCursor.substr(0, end);
            // Checked first: with no prefix, the correct shape prefix + added matches a dropped deletion too.
            if (!deleted.empty() && isEndsWith(shown, prefix + deleted + added))
                return SurroundingVerdict::DroppedDeletion;
            if (isEndsWith(shown, prefix + added))
                return SurroundingVerdict::Correct;
            if (!added.empty() && isEndsWith(shown, added))
                return SurroundingVerdict::WrongEarlierPart;
            if (end == 0)
                break;
            do
                --end;
            while (end > 0 && (static_cast<unsigned char>(beforeCursor[end]) & 0xC0) == 0x80);
        }
        return SurroundingVerdict::NoAnswer;
    }

    const char* verdictName(SurroundingVerdict verdict) {
        switch (verdict) {
            case SurroundingVerdict::DroppedDeletion: return "app dropped the deletion";
            case SurroundingVerdict::Correct: return "correct";
            case SurroundingVerdict::WrongEarlierPart: return "commit seen, text before it wrong";
            case SurroundingVerdict::NoAnswer: return "not answered";
        }
        return "";
    }

} // namespace ngosen
