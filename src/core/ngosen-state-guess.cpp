/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"
#include <string>

namespace ngosen {

    std::optional<TypingState::GuessedCommit> TypingState::takeGuess(const std::string& commit, const std::string& wordInField) {
        if (diacritics_ == 0)
            return std::nullopt;
        const UniqueCPtr<char> pulled(EnginePullChanges(bambooEngine_.handle()));
        // The old text's byte length comes first, since a macro can put a tab in either text.
        const std::string_view changes = pulled ? pulled.get() : "";
        const size_t           tab     = changes.find('\t');
        if (tab == std::string_view::npos)
            return std::nullopt;
        const size_t           oldSize = std::stoul(std::string(changes.substr(0, tab)));
        const std::string_view texts   = changes.substr(tab + 1);
        const std::string      old(texts.substr(0, oldSize));
        if (!isEndsWith(old, commit)) {
            NGOSEN_INFO("Guess: kept as typed, the commit is not where the changes end");
            dropGuessRun();
            return std::nullopt;
        }
        GuessedCommit guess{.before = old.substr(0, old.size() - commit.size()), .text = std::string(texts.substr(old.size()))};
        if (!fieldShowsGuessed(guess.before, wordInField)) {
            NGOSEN_INFO("Guess: kept as typed, the field no longer shows the words");
            dropGuessRun();
            return std::nullopt;
        }
        NGOSEN_INFO(guess.before.empty() ? "Guess: the last word" : "Guess: earlier words");
        return guess;
    }

    // The last word may change wherever the field could still show it; earlier words only where it does.
    bool TypingState::fieldShowsGuessed(const std::string& before, const std::string& wordInField) const {
        if (before.empty())
            return typedBeforeCursor(wordInField);
        const Surrounding s = host_->surrounding();
        return reportsTextInTime() && s.isValid() && !s.text().empty() && s.cursor() == s.anchor() && endsBeforeCursor(s, spelledAtWordEnd(before) + wordInField, false);
    }

    // The engine believes its changes reached the field; ending the run keeps it from building on them.
    void TypingState::dropGuessRun() {
        if (diacritics_ == 0)
            return;
        EngineSetDiacritics(bambooEngine_.handle(), 0);
        EngineSetDiacritics(bambooEngine_.handle(), diacritics_);
    }

    std::string TypingState::commitGuessInPreedit(const std::string& text, bool keyTaken) {
        auto guess = takeGuess(text, "");
        // A key the app gets would land before the replacement finishes.
        if (guess && !guess->before.empty() && !keyTaken) {
            NGOSEN_INFO("Guess: kept as typed, the key goes to the app");
            dropGuessRun();
            guess.reset();
        }
        if (!guess) {
            commit(text);
            return text;
        }
        if (guess->before.empty()) {
            commit(guess->text);
            return guess->text;
        }
        std::string deletedPart;
        std::string addedPart;
        utf8::compareAndSplitStrings(guess->before, guess->text, deletedPart, addedPart);
        performReplacement(guess->before, deletedPart, addedPart);
        // Undoing the last word alone would take the changed earlier words with it.
        return "";
    }

} // namespace ngosen
