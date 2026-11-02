/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <vector>

namespace ngosen {

    // How many characters to ask a frontend to delete when it turns the count into bytes using its
    // own copy of the text, which can lag behind what the app holds.
    struct SurroundingDeletion {
        enum class Match {
            Fresh,   // the copy ends with the word as we know it
            OldTail, // the copy still shows an earlier state of this word
            None,    // the copy says nothing about this word
        };
        Match        match = Match::None;
        unsigned int chars = 0;
        // Text the deletion removes before the deleted part, to commit again ahead of the replacement.
        std::string retype;
        // The copy turns chars into a different number of bytes than the deleted part has, so the
        // deletion would go wrong until the copy catches up.
        bool wrongBytes = false;
    };

    // copyBeforeCursor is the frontend's copy up to the cursor; prefix + deleted is the word the app
    // holds there; earlierWords are the states this word had before earlier replacements.
    SurroundingDeletion countSurroundingDeletion(const std::string& copyBeforeCursor, const std::string& prefix, const std::string& deleted,
                                                 const std::vector<std::string>& earlierWords);

} // namespace ngosen
