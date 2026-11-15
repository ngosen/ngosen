/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-emoji-keywords.h"

#include "bamboo-core.h"
#include "ngosen-go-object.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace ngosen {

    namespace {

        struct Entry {
            const char* keyword;
            const char* emoji;
            EmojiKind   kind;
        };

        const Entry entries[] = {
#include "ngosen-emoji-keywords.inc"
        };

        std::string convert(const std::string& text, int letterCase, bool stripDiacritics) {
            UniqueCPtr<char> out(ConvertText(text.c_str(), "Unicode", "Unicode", letterCase, stripDiacritics));
            return out ? std::string(out.get()) : text;
        }

        std::string lower(const std::string& text) {
            return convert(text, 2, false);
        }

        std::string fold(const std::string& text) {
            return convert(text, 0, true);
        }

        bool isAscii(const std::string& text) {
            return std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
        }

        // Nobody counts the k in "kkkk" or the ha in "hahaha".
        std::string collapseRepeats(const std::string& word) {
            if (!isAscii(word))
                return word;
            std::string out;
            for (char c : word) {
                if (out.size() >= 2 && out[out.size() - 1] == c && out[out.size() - 2] == c)
                    continue;
                out += c;
            }
            while (out.size() >= 6 && out.compare(out.size() - 2, 2, out, out.size() - 4, 2) == 0 && out.compare(out.size() - 4, 2, out, out.size() - 6, 2) == 0)
                out.erase(out.size() - 2);
            return out;
        }

        std::vector<std::string> split(const char* emoji) {
            std::vector<std::string> out;
            std::string              current;
            for (const char* p = emoji; *p; ++p) {
                if (*p == ' ') {
                    out.push_back(std::move(current));
                    current.clear();
                } else {
                    current += *p;
                }
            }
            if (!current.empty())
                out.push_back(std::move(current));
            return out;
        }

        struct Index {
            std::unordered_map<std::string, const Entry*>              exact;
            std::unordered_map<std::string, std::vector<const Entry*>> folded;
            size_t                                                     maxWords = 1;

            Index() {
                for (const auto& entry : entries) {
                    const std::string keyword = entry.keyword;
                    exact.emplace(keyword, &entry);
                    folded[fold(keyword)].push_back(&entry);
                    maxWords = std::max<size_t>(maxWords, std::count(keyword.begin(), keyword.end(), ' ') + 1);
                }
            }
        };

        const Index& index() {
            static const Index built;
            return built;
        }

        bool isVietnamese(const std::string& word) {
            return IsVietnameseSyllable(word.c_str()) != 0;
        }

        std::vector<const Entry*> lookup(const std::vector<std::string>& typed, const std::vector<std::string>& lowered) {
            const auto& idx = index();
            std::string phrase;
            for (const auto& word : lowered)
                phrase += (phrase.empty() ? "" : " ") + word;
            if (auto it = idx.exact.find(phrase); it != idx.exact.end())
                return {it->second};
            if (lowered.size() == 1) {
                if (auto it = idx.exact.find(collapseRepeats(phrase)); it != idx.exact.end() && it->second->kind == EmojiKind::Sound)
                    return {it->second};
            }
            if (!isAscii(phrase) || std::all_of(typed.begin(), typed.end(), isVietnamese))
                return {};
            if (auto it = idx.folded.find(phrase); it != idx.folded.end())
                return it->second;
            return {};
        }

        size_t firstCharLength(const std::string& text) {
            if (text.empty())
                return 0;
            const auto   lead = static_cast<unsigned char>(text[0]);
            const size_t n    = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            return std::min(n, text.size());
        }

        // The whole first letter, not its first byte: Đ and đ share their first byte.
        bool startsCapitalized(const std::string& word) {
            const std::string lowered = lower(word);
            const size_t      n       = firstCharLength(word);
            return n > 0 && word.compare(0, n, lowered, 0, firstCharLength(lowered)) != 0;
        }

    } // namespace

    std::optional<EmojiSuggestion> suggestEmoji(const std::vector<std::string>& words, bool yahooEmoticons) {
        const size_t most = std::min(words.size(), index().maxWords);
        for (size_t n = most; n > 0; --n) {
            const std::vector<std::string> typed(words.end() - static_cast<std::ptrdiff_t>(n), words.end());
            std::vector<std::string>       lowered;
            for (const auto& word : typed)
                lowered.push_back(lower(word));
            EmojiSuggestion suggestion;
            for (const Entry* entry : lookup(typed, lowered)) {
                if (entry->kind == EmojiKind::Country && !startsCapitalized(typed.front()))
                    continue;
                if (entry->kind == EmojiKind::Yahoo && !yahooEmoticons)
                    continue;
                if (suggestion.emoji.empty()) {
                    suggestion.keyword = entry->keyword;
                    suggestion.kind    = entry->kind == EmojiKind::Yahoo ? EmojiKind::Emoticon : entry->kind;
                }
                for (auto& emoji : split(entry->emoji)) {
                    if (suggestion.emoji.size() < 5 && std::find(suggestion.emoji.begin(), suggestion.emoji.end(), emoji) == suggestion.emoji.end())
                        suggestion.emoji.push_back(std::move(emoji));
                }
            }
            if (!suggestion.emoji.empty()) {
                suggestion.words = n;
                return suggestion;
            }
        }
        return std::nullopt;
    }

} // namespace ngosen
