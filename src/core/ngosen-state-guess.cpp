/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-app-quirks.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"
#include <string>
#include <utility>

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

    bool TypingState::guessesHere() const {
        return engine_->options().guessInTerminals || !ngosen::showsNoTypedText(host_->field());
    }

    // The engine believes its changes reached the field; ending the run keeps it from building on them.
    void TypingState::dropGuessRun() {
        if (diacritics_ == 0)
            return;
        EngineSetDiacritics(bambooEngine_.handle(), 0);
        EngineSetDiacritics(bambooEngine_.handle(), diacritics_);
    }

    // Backspace right after a fixed word: the field takes the fix back and the word reopens as typed.
    std::optional<std::string> TypingState::takeBackFix() {
        const Surrounding s = host_->surrounding();
        if (diacritics_ == 0 || lastWord_.empty() || !reportsTextInTime() || !s.isValid() || s.cursor() != s.anchor() || !endsBeforeCursor(s, lastWord_, false))
            return std::nullopt;
        const UniqueCPtr<char> shown(EngineUndoFix(bambooEngine_.handle()));
        if (!shown)
            return std::nullopt;
        // The engine already reopened the word; it must not stay open over text the field does not
        // show. Backspace still reopens the commit as it would without Đoán dấu.
        if (spelledAtWordEnd(shown.get()) != lastWord_) {
            NGOSEN_INFO("Undo fix: kept, the field shows another word");
            ResetEngine(bambooEngine_.handle());
            return std::nullopt;
        }
        lastWord_.clear();
        droppedWord_.clear();
        NGOSEN_INFO("Undo fix");
        const UniqueCPtr<char> typed(EnginePullPreedit(bambooEngine_.handle()));
        return std::string(typed ? typed.get() : "");
    }

    bool TypingState::undoFixInField(ngosen::KeyPress& keyEvent) {
        if (!oldPreBuffer_.empty() || !canSendBackspaces())
            return false;
        const std::string committed = lastWord_;
        const auto        typed     = takeBackFix();
        if (!typed)
            return false;
        std::string deletedPart;
        std::string addedPart;
        utf8::compareAndSplitStrings(committed, *typed, deletedPart, addedPart);
        is_deleting_.store(false, std::memory_order_release);
        keyEvent.accept();
        performReplacement(oldPreBuffer_, deletedPart, addedPart);
        oldPreBuffer_ = *typed;
        hasHistory_   = true;
        return true;
    }

    bool TypingState::undoFixInPreedit(ngosen::KeyPress& keyEvent) {
        const size_t length = utf8::length(lastWord_);
        const auto   typed  = takeBackFix();
        if (!typed)
            return false;
        keyEvent.accept();
        deleteBeforeCursor(length);
        host_->showPreedit(*typed, false);
        host_->refreshPreedit();
        return true;
    }

    // Backspace right after a word that made Đoán dấu fix the word before it: what the field shows
    // from the fixed word to the cursor, and what was typed there.
    std::optional<std::pair<std::string, std::string>> TypingState::takeBackEarlierFix() {
        if (diacritics_ == 0 || !reportsTextInTime())
            return std::nullopt;
        const UniqueCPtr<char> shown(EngineEarlierFixShown(bambooEngine_.handle()));
        if (!shown)
            return std::nullopt;
        const std::string fixed = spelledAtWordEnd(shown.get());
        const Surrounding s     = host_->surrounding();
        // Taking the fix back teaches the word as typed, so not when the field shows other words.
        if (!s.isValid() || s.cursor() != s.anchor() || !endsBeforeCursor(s, fixed, false))
            return std::nullopt;
        const UniqueCPtr<char> typed(EngineUndoEarlierFix(bambooEngine_.handle()));
        if (!typed)
            return std::nullopt;
        lastWord_.clear();
        droppedWord_.clear();
        NGOSEN_INFO("Undo fix: the word before");
        return std::pair{fixed, spelledAtWordEnd(typed.get())};
    }

    bool TypingState::undoEarlierFixInField(ngosen::KeyPress& keyEvent) {
        if (!oldPreBuffer_.empty() || !canSendBackspaces())
            return false;
        const auto change = takeBackEarlierFix();
        if (!change)
            return false;
        std::string deletedPart;
        std::string addedPart;
        utf8::compareAndSplitStrings(change->first, change->second, deletedPart, addedPart);
        is_deleting_.store(false, std::memory_order_release);
        keyEvent.accept();
        performReplacement(change->first, deletedPart, addedPart);
        return true;
    }

    bool TypingState::undoEarlierFixInPreedit(ngosen::KeyPress& keyEvent) {
        const auto change = takeBackEarlierFix();
        if (!change)
            return false;
        std::string deletedPart;
        std::string addedPart;
        utf8::compareAndSplitStrings(change->first, change->second, deletedPart, addedPart);
        keyEvent.accept();
        deleteBeforeCursor(utf8::length(deletedPart));
        commit(addedPart);
        return true;
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
        // At once, as Backspace into a word does here: a replacement that waits would queue the next
        // keys, and their replay knows nothing of the preedit.
        deleteBeforeCursor(utf8::length(deletedPart));
        commit(addedPart);
        // Undoing the last word alone would take the changed earlier words with it.
        return "";
    }

} // namespace ngosen
