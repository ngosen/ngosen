/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-options.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ngosen {

    enum class EmojiKind {
        Word,     // the emoji goes after the word
        Sound,    // haha, huhu: the emoji stands in for it
        Emoticon, // :), <3
        Country,  // offered only for a capitalized name, so "biện pháp" never gets a flag
        Yahoo,    // a Yahoo! Messenger emoticon, offered as an Emoticon when those are on
    };

    struct EmojiSuggestion {
        std::string              keyword;
        size_t                   words = 0; // typed words the keyword covers, counted from the last
        std::vector<std::string> emoji;     // best first
        EmojiKind                kind = EmojiKind::Word;
    };

    // The emoji for the longest keyword the typed words end with. Words are as typed, the last one
    // at the back; a word without marks matches a marked keyword only when it is not Vietnamese
    // as written, so "cuoi" offers 😂 and "cho" offers nothing. The user's edits come before the defaults.
    std::optional<EmojiSuggestion> suggestEmoji(const std::vector<std::string>& words, bool yahooEmoticons, const std::vector<EmojiWordEdit>& edits);

} // namespace ngosen
