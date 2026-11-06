/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-surrounding-count.h"

#include "ngosen-strings.h"
#include "ngosen-utf8.h"

namespace ngosen {

    namespace {
        bool startsChar(const std::string& text, size_t at) {
            return at >= text.size() || (static_cast<unsigned char>(text[at]) & 0xc0) != 0x80;
        }

        // Bytes of the last chars characters of text, or npos if text is shorter.
        size_t bytesOfLastChars(const std::string& text, unsigned int chars) {
            size_t start = text.size();
            for (unsigned int n = 0; n < chars; ++n) {
                if (start == 0)
                    return std::string::npos;
                do
                    --start;
                while (start > 0 && !startsChar(text, start));
            }
            return text.size() - start;
        }

        bool lacksLastKeys(const std::string& copyBeforeCursor, const std::string& prefix, const std::string& deleted) {
            std::string shorter = prefix + deleted;
            for (size_t left = shorter.size(); left > 0;) {
                do
                    --left;
                while (left > 0 && !startsChar(shorter, left));
                shorter.resize(left);
                if (!shorter.empty() && isEndsWith(copyBeforeCursor, shorter))
                    return true;
            }
            return false;
        }

        SurroundingDeletion unmatched(SurroundingDeletion result, const std::string& copyBeforeCursor, const std::string& prefix, const std::string& deleted) {
            result.wrongBytes = bytesOfLastChars(copyBeforeCursor, result.chars) != deleted.size();
            result.behind     = lacksLastKeys(copyBeforeCursor, prefix, deleted);
            return result;
        }
    } // namespace

    SurroundingDeletion countSurroundingDeletion(const std::string& copyBeforeCursor, const std::string& prefix, const std::string& deleted,
                                                 const std::vector<std::string>& earlierWords) {
        SurroundingDeletion result;
        result.chars            = static_cast<unsigned int>(utf8::length(deleted));
        const std::string known = prefix + deleted;
        if (isEndsWith(copyBeforeCursor, known)) {
            result.match = SurroundingDeletion::Match::Fresh;
            return result;
        }
        // Ace keeps a line break after its line and the copy's cursor can stay past it after a commit,
        // so a deletion counted from there would take the line break and leave the letter. Ace puts the
        // line break back itself and shows a committed one as a new line.
        if (isEndsWith(copyBeforeCursor, known + "\n")) {
            result.match         = SurroundingDeletion::Match::Fresh;
            result.chars         = result.chars + 1;
            result.pastLineBreak = true;
            return result;
        }
        bool shown = false;
        for (const auto& word : earlierWords)
            shown = shown || (!word.empty() && isEndsWith(copyBeforeCursor, word));
        if (!shown)
            return unmatched(result, copyBeforeCursor, prefix, deleted);

        // The frontend deletes the bytes of the last n characters of its copy; pick the smallest n
        // whose bytes cover the deleted part and end on a character boundary of the real text.
        const size_t target = deleted.size();
        size_t       start  = copyBeforeCursor.size();
        for (unsigned int n = 1; start > 0; ++n) {
            do
                --start;
            while (start > 0 && !startsChar(copyBeforeCursor, start));
            const size_t bytes = copyBeforeCursor.size() - start;
            if (bytes > known.size())
                break;
            if (bytes < target || !startsChar(known, known.size() - bytes))
                continue;
            result.match  = SurroundingDeletion::Match::OldTail;
            result.chars  = n;
            result.retype = known.substr(known.size() - bytes, bytes - target);
            return result;
        }
        return unmatched(result, copyBeforeCursor, prefix, deleted);
    }

} // namespace ngosen
