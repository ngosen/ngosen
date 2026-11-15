/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: emoji offered for the words just typed (#110).
#include "ngosen-clock.h"
#include "ngosen-emoji-keywords.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ngosen {

    namespace {
        // Enough for the longest keyword; older text is dropped a word at a time.
        constexpr size_t   MaxEmojiTail = 64;
        constexpr uint64_t RecheckUs    = 20000;

        bool               isShortcut(const ngosen::KeyPress& key) {
            return (key.states() & (ngosen::modifier::Ctrl | ngosen::modifier::Alt | ngosen::modifier::Super)) != 0U;
        }

        std::vector<std::string> splitWords(const std::string& text) {
            std::vector<std::string> words;
            size_t                   start = 0;
            while (start < text.size()) {
                size_t end = text.find(' ', start);
                if (end == std::string::npos)
                    end = text.size();
                if (end > start)
                    words.push_back(text.substr(start, end - start));
                start = end + 1;
            }
            return words;
        }

        bool replacesTyped(const ngosen::EmojiSuggestion& suggestion) {
            return suggestion.kind == ngosen::EmojiKind::Sound || suggestion.kind == ngosen::EmojiKind::Emoticon;
        }
    } // namespace

    std::string TypingState::openWord() {
        if (!bambooEngine_)
            return {};
        if (mode() != ngosen::Mode::Preedit)
            return oldPreBuffer_;
        ngosen::UniqueCPtr<char> preedit(EnginePullPreedit(bambooEngine_.handle()));
        return preedit ? preedit.get() : "";
    }

    bool TypingState::offersEmojiHere() const {
        if (!bambooEngine_ || !engine_->options().emojiSuggestions || !emojiOn_ || retype_)
            return false;
        if (mode() != ngosen::Mode::Sen && mode() != ngosen::Mode::Preedit)
            return false;
        if (vimOn() && vimState_ != VimState::Insert)
            return false;
        // Tab is held only where the panel is known to show the hint, so it never vanishes unseen.
        const Field field = host_->field();
        return field.drawsCandidates && !field.hidesText() && !field.terminal && !inAddressBar();
    }

    void TypingState::setEmojiOn(bool on) {
        emojiOn_ = on;
        if (!on) {
            emojiTail_.clear();
            dropEmojiOffer();
        }
    }

    void TypingState::noteTypedForEmoji(const ngosen::KeyPress& keyEvent, const std::string& wordBefore) {
        const uint32_t sym = keyEvent.sym();
        if (clickBeforeKey_ || isShortcut(keyEvent)) {
            emojiTail_.clear();
            return;
        }
        if (ngosen::key::isBackspace(sym)) {
            if (wordBefore.empty())
                ngosen::utf8::eraseLastCodepoint(emojiTail_);
            return;
        }
        const std::string text = host_->keyText(sym);
        if (text.empty() || static_cast<unsigned char>(text[0]) < 0x20) {
            emojiTail_.clear();
            return;
        }
        // The key ended the word, or typed a character outside one, as the ) of :).
        if (openWord().empty())
            emojiTail_ += wordBefore + text;
        if (emojiTail_.size() > MaxEmojiTail) {
            const size_t cut = emojiTail_.find(' ', emojiTail_.size() - MaxEmojiTail);
            emojiTail_.erase(0, cut == std::string::npos ? emojiTail_.size() : cut + 1);
        }
    }

    bool TypingState::replacementPending() const {
        return is_deleting_.load(std::memory_order_acquire) || key_report_pending_ || edit_hold_ || split_edit_pending_ || surr_wait_pending_ || overtype_pending_;
    }

    void TypingState::updateEmojiOffer() {
        emojiRecheck_.reset();
        // The hint waits for the word to reach the app, so the emoji cannot land inside it.
        if (replacementPending()) {
            dropEmojiOffer();
            emojiRecheck_ = host_->startTimer(ngosen::monotonicUs() + RecheckUs, RecheckUs, [this](ngosen::Timer& timer) {
                if (!replacementPending()) {
                    offerForTypedText();
                    return false;
                }
                timer.rearm(ngosen::monotonicUs() + RecheckUs);
                return true;
            });
            return;
        }
        offerForTypedText();
    }

    void TypingState::offerForTypedText() {
        if (!offersEmojiHere()) {
            emojiTail_.clear();
            dropEmojiOffer();
            return;
        }
        const std::string word  = openWord();
        const std::string text  = emojiTail_ + word;
        const auto        words = splitWords(text);
        auto              found = text.empty() || text.back() == ' ' ? std::nullopt : suggestEmoji(words, engine_->options().yahooEmoticons, engine_->options().emojiWordEdits);
        if (!found) {
            dropEmojiOffer();
            return;
        }
        const std::string& covered = words.back();
        // A sound or an emoticon is deleted, which needs it all in the preedit or a way to delete in the app.
        if (replacesTyped(*found) && (mode() == ngosen::Mode::Preedit ? covered != word : !canReplaceInApp())) {
            dropEmojiOffer();
            return;
        }
        // As typed, with every space between its words, for the length to delete.
        size_t start = text.size();
        for (size_t n = 0; n < found->words; ++n) {
            start = text.find_last_not_of(' ', start - 1);
            start = text.find_last_of(' ', start);
            start = start == std::string::npos ? 0 : start + 1;
        }
        std::string keyword = text.substr(start);
        emojiOffer_ = EmojiOffer{.suggestion = std::move(*found), .covered = covered, .keyword = std::move(keyword)};
        showEmojiOffer();
    }

    void TypingState::showEmojiOffer() {
        const auto& emoji = emojiOffer_->suggestion.emoji;
        if (!emojiOffer_->picking) {
            std::string hint = "Tab:";
            for (const auto& e : emoji)
                hint += " " + e;
            host_->showCandidates({hint}, 1, [this](size_t) {
                if (emojiOffer_ && !replacementPending())
                    insertEmoji(0);
            });
        } else {
            std::vector<std::string> labels;
            for (size_t i = 0; i < emoji.size(); ++i)
                labels.push_back(std::to_string(i + 1) + ". " + emoji[i]);
            host_->showCandidates(labels, static_cast<int>(labels.size()), [this](size_t index) {
                if (emojiOffer_ && !replacementPending() && index < emojiOffer_->suggestion.emoji.size())
                    insertEmoji(index);
            });
            const std::string numbers = emoji.size() > 1 ? "1–" + std::to_string(emoji.size()) : "1";
            host_->setStatus(numbers + ": " + host_->translate("pick, Space or Enter: the first, Esc: keep the word"));
        }
        host_->refreshPanel();
    }

    void TypingState::dropEmojiOffer() {
        if (!emojiOffer_)
            return;
        if (emojiOffer_->picking)
            host_->setStatus({});
        emojiOffer_.reset();
        host_->hideCandidates();
        host_->refreshPanel();
    }

    void TypingState::takeEmojiKey(ngosen::KeyPress& keyEvent) {
        keyEvent.accept();
        emojiTookPress_ = true;
        emojiTakenSyms_.push_back(keyEvent.sym());
    }

    bool TypingState::handleEmojiOfferKey(ngosen::KeyPress& keyEvent) {
        const uint32_t sym = keyEvent.sym();
        if (keyEvent.isRelease()) {
            const auto taken = std::find(emojiTakenSyms_.begin(), emojiTakenSyms_.end(), sym);
            if (taken == emojiTakenSyms_.end())
                return false;
            emojiTakenSyms_.erase(taken);
            keyEvent.accept();
            return true;
        }
        if (keyEvent.isModifier() || !emojiOffer_)
            return false;
        // A click moved the cursor away from the word, and the mode or field may have changed since.
        const bool clicked = g_mouse_clicked.load(std::memory_order_acquire) && !is_deleting_.load(std::memory_order_acquire);
        if (clicked || replacementPending() || !offersEmojiHere()) {
            dropEmojiOffer();
            return false;
        }
        const bool plain = !keyEvent.hasModifier();
        if (!emojiOffer_->picking) {
            if (sym != ngosen::key::Tab || !plain)
                return false;
            takeEmojiKey(keyEvent);
            emojiOffer_->picking = true;
            showEmojiOffer();
            return true;
        }
        const size_t count = emojiOffer_->suggestion.emoji.size();
        if (plain && sym >= ngosen::key::Digit1 && sym < ngosen::key::Digit1 + count) {
            takeEmojiKey(keyEvent);
            insertEmoji(sym - ngosen::key::Digit1);
            return true;
        }
        if (plain && (sym == ngosen::key::Tab || sym == ngosen::key::space || ngosen::key::isEnter(sym))) {
            takeEmojiKey(keyEvent);
            insertEmoji(0);
            return true;
        }
        if (plain && sym == ngosen::key::Escape) {
            takeEmojiKey(keyEvent);
            dropEmojiOffer();
            return true;
        }
        // Any other key closes the list and is typed as usual.
        dropEmojiOffer();
        return false;
    }

    void TypingState::insertEmoji(size_t index) {
        const EmojiOffer offer = std::move(*emojiOffer_);
        dropEmojiOffer();
        const std::string& emoji    = offer.suggestion.emoji[index];
        const bool         replaces = replacesTyped(offer.suggestion);
        const std::string  word     = openWord();
        const bool         preedit  = mode() == ngosen::Mode::Preedit;
        NGOSEN_INFO("Emoji: " + logged(offer.covered) + " -> " + emoji);
        ResetEngine(bambooEngine_.handle());
        hasHistory_ = false;
        lastWord_.clear();
        emojiTail_.clear();
        if (preedit) {
            host_->clearPreedit();
            host_->refreshPreedit();
            if (replaces)
                commit(emoji);
            else if (!replacesKeyword(offer, word, emoji))
                commit(word + " " + emoji);
            return;
        }
        // The word is already in the app.
        oldPreBuffer_.clear();
        if (replaces)
            replaceRetyped(offer.covered, emoji);
        else if (!replacesKeyword(offer, {}, emoji))
            commit(" " + emoji);
    }

    // With the emoji set to stand in for the word, takes out what the app holds of the keyword.
    bool TypingState::replacesKeyword(const EmojiOffer& offer, const std::string& inPreedit, const std::string& emoji) {
        if (!engine_->options().emojiReplacesWord || !isEndsWith(offer.keyword, inPreedit))
            return false;
        const std::string inApp = offer.keyword.substr(0, offer.keyword.size() - inPreedit.size());
        if (inApp.empty()) {
            commit(emoji);
            return true;
        }
        return canReplaceInApp() && replaceRetyped(inApp, emoji);
    }

} // namespace ngosen
