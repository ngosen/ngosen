/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-surrounding-check.h"

namespace ngosen {

    namespace {
        bool endsWith(const std::string& text, const std::string& tail) {
            return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
    } // namespace

    SurroundingVerdict judgeSurroundingReport(const std::string& beforeCursor, const std::string& prefix, const std::string& deleted, const std::string& added,
                                              const std::string& typedAfter) {
        // A lagging report shows only the first of the keys typed since, so try each, longest first.
        for (size_t typed = typedAfter.size() + 1; typed-- > 0;) {
            if (typed < typedAfter.size() && (static_cast<unsigned char>(typedAfter[typed]) & 0xC0) == 0x80)
                continue;
            const std::string after = typedAfter.substr(0, typed);
            // Checked first: with no prefix, the correct shape prefix + added matches a dropped deletion too.
            if (!deleted.empty() && endsWith(beforeCursor, prefix + deleted + added + after))
                return SurroundingVerdict::DroppedDeletion;
            if (endsWith(beforeCursor, prefix + added + after))
                return SurroundingVerdict::Correct;
            if (!added.empty() && endsWith(beforeCursor, added + after))
                return SurroundingVerdict::WrongEarlierPart;
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
