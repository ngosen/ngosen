/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-surrounding-count.h"

#include "ngosen-utf8.h"

namespace ngosen {

    namespace {
        bool endsWith(const std::string& text, const std::string& tail) {
            return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }

        bool startsChar(const std::string& text, size_t at) {
            return at >= text.size() || (static_cast<unsigned char>(text[at]) & 0xc0) != 0x80;
        }

        // Byte offsets in text where each character starts, plus text.size().
        std::vector<size_t> charStarts(const std::string& text) {
            std::vector<size_t> starts;
            for (size_t at = 0; at < text.size();) {
                starts.push_back(at);
                const size_t step = utf8::charBytes(std::string_view(text).substr(at));
                at += step == 0 ? 1 : step;
            }
            starts.push_back(text.size());
            return starts;
        }
    } // namespace

    SurroundingDeletion countSurroundingDeletion(const std::string& copyBeforeCursor, const std::string& prefix, const std::string& deleted,
                                                 const std::vector<std::string>& earlierWords) {
        SurroundingDeletion result;
        result.chars            = static_cast<unsigned int>(utf8::length(deleted));
        const std::string known = prefix + deleted;
        if (endsWith(copyBeforeCursor, known)) {
            result.match = SurroundingDeletion::Match::Fresh;
            return result;
        }
        bool shown = false;
        for (const auto& word : earlierWords)
            shown = shown || (!word.empty() && endsWith(copyBeforeCursor, word));
        if (!shown)
            return result;

        // The frontend deletes the bytes of the last n characters of its copy; pick the smallest n
        // whose bytes cover the deleted part and end on a character boundary of the real text.
        const size_t target = deleted.size();
        const auto   starts = charStarts(copyBeforeCursor);
        for (size_t n = 1; n < starts.size(); ++n) {
            const size_t bytes = copyBeforeCursor.size() - starts[starts.size() - 1 - n];
            if (bytes > known.size())
                break;
            if (bytes < target || !startsChar(known, known.size() - bytes))
                continue;
            result.match  = SurroundingDeletion::Match::OldTail;
            result.chars  = static_cast<unsigned int>(n);
            result.retype = known.substr(known.size() - bytes, bytes - target);
            return result;
        }
        return result;
    }

} // namespace ngosen
