/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: offering to retype, with marks, what was typed while another input method was on.
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-utf8.h"

#include <string>

namespace ngosen {

    namespace {
        // Enough for a sentence; older text is dropped a word at a time.
        constexpr size_t MaxTypedWhileOff = 200;
        // Qt drops backspaces forwarded while it is still handling the switch key.
        constexpr uint64_t RetypeDelayUs = 100000;

        bool               isPrintableAscii(uint32_t sym) {
            return sym >= 0x20 && sym <= 0x7e;
        }

        // Shift only picks the character, so it does not count.
        bool isShortcut(const ngosen::KeyPress& key) {
            return (key.states() & (ngosen::modifier::Ctrl | ngosen::modifier::Alt | ngosen::modifier::Super)) != 0U;
        }
    } // namespace

    void TypingState::noteKeyWhileOff(const ngosen::KeyPress& key) {
        if (key.isRelease() || key.isModifier())
            return;
        if (!engine_->options().retypeAfterSwitch) {
            typedWhileOff_.clear();
            return;
        }
        // A click moved the cursor, so the text before it is no longer what was typed.
        if (g_mouse_clicked.exchange(false, std::memory_order_acq_rel))
            typedWhileOff_.clear();
        const uint32_t sym = key.sym();
        if (host_->field().password || isShortcut(key)) {
            typedWhileOff_.clear();
        } else if (ngosen::key::isBackspace(sym)) {
            if (!typedWhileOff_.empty())
                typedWhileOff_.pop_back();
        } else if (isPrintableAscii(sym)) {
            typedWhileOff_ += static_cast<char>(sym);
            if (typedWhileOff_.size() > MaxTypedWhileOff) {
                const size_t cut = typedWhileOff_.find(' ', typedWhileOff_.size() - MaxTypedWhileOff);
                typedWhileOff_.erase(0, cut == std::string::npos ? typedWhileOff_.size() : cut + 1);
            }
        } else {
            typedWhileOff_.clear();
        }
    }

    void TypingState::forgetKeysWhileOff() {
        typedWhileOff_.clear();
    }

    void TypingState::fieldResetWhileOff() {
        // Firefox resets after every key. Where the field reports its text, offerRetype checks it instead.
        if (!reportsTextInTime())
            forgetKeysWhileOff();
    }

    std::string TypingState::addMarks(const std::string& typed) {
        std::string out;
        ResetEngine(bambooEngine_.handle());
        for (const char c : typed) {
            const bool               processed = EngineProcessKeyEvent(bambooEngine_.handle(), static_cast<unsigned char>(c), 0) != 0U;
            ngosen::UniqueCPtr<char> commit(EnginePullCommit(bambooEngine_.handle()));
            if (commit)
                out += commit.get();
            if (!processed)
                out += c;
        }
        EngineCommitPreedit(bambooEngine_.handle());
        ngosen::UniqueCPtr<char> rest(EnginePullCommit(bambooEngine_.handle()));
        if (rest)
            out += rest.get();
        ResetEngine(bambooEngine_.handle());
        return out;
    }

    bool TypingState::everyWordTakesMarks(const std::string& typed) {
        size_t words = 0;
        for (size_t start = 0; start < typed.size();) {
            size_t end = typed.find(' ', start);
            if (end == std::string::npos)
                end = typed.size();
            if (end > start) {
                const std::string word = typed.substr(start, end - start);
                if (addMarks(word) == word)
                    return false;
                ++words;
            }
            start = end + 1;
        }
        return words > 0;
    }

    bool TypingState::reportsTextInTime() const {
        return host_->field().surroundingText && !ngosen::surroundingTextLags(host_->field());
    }

    bool TypingState::typedBeforeCursor(const std::string& typed) const {
        const Surrounding s = host_->surrounding();
        // Some apps (kitty, Konsole) declare surrounding text but always send it empty.
        if (!reportsTextInTime() || !s.isValid() || s.text().empty())
            return true;
        const std::string& t  = s.text();
        auto               it = t.begin();
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i)
            it = ngosen::utf8::nextChar(it, t.end());
        std::string before(t.begin(), it);
        // Firefox contenteditable reports a trailing space as U+00A0.
        for (size_t pos = 0; (pos = before.find("\u00a0", pos)) != std::string::npos;)
            before.replace(pos, 2, " ");
        // xterm.js keeps only the last key in its hidden textarea: a field holding just the end of what we typed.
        if (before.size() < typed.size() && it == t.end())
            return typed.compare(typed.size() - before.size(), before.size(), before) == 0;
        return before.size() >= typed.size() && before.compare(before.size() - typed.size(), typed.size(), typed) == 0;
    }

    void TypingState::offerRetype() {
        const std::string typed = std::move(typedWhileOff_);
        typedWhileOff_.clear();
        retype_.reset();
        if (!bambooEngine_ || !engine_->options().retypeAfterSwitch || typed.empty() || (realMode != ngosen::Mode::Sen && realMode != ngosen::Mode::Preedit))
            return;
        // Without either, Tab could not remove what was typed.
        if (!canSendBackspaces() && !host_->field().surroundingText)
            return;
        if (!typedBeforeCursor(typed))
            return;
        std::string marked = addMarks(typed);
        if (marked == typed)
            return;
        // A word Telex leaves alone is likely English typed on purpose, so then only offer.
        if (everyWordTakesMarks(typed)) {
            NGOSEN_INFO("Retype: " + typed + " -> " + marked);
            retype_       = Retype{.typed = typed, .marked = marked, .atOnce = true};
            retype_timer_ = host_->startTimer(ngosen::monotonicUs() + RetypeDelayUs, 0, [this](ngosen::Timer&) {
                if (retype_ && retype_->atOnce && !retype_->applied) {
                    applyRetype();
                    host_->setStatus(host_->translate("Esc: put back what was typed"));
                    host_->refreshPanel();
                }
                return false;
            });
            return;
        }
        NGOSEN_INFO("Offer retype: " + typed + " -> " + marked);
        retype_ = Retype{.typed = typed, .marked = marked};
        host_->showCandidates({typed + " → " + marked}, 1, [this](size_t) { applyRetype(); });
        host_->setStatus(host_->translate("Tab: retype in Vietnamese, Esc: keep"));
        host_->refreshPanel();
    }

    void TypingState::applyRetype() {
        if (!retype_)
            return;
        hideOffer();
        const bool         undo = retype_->applied;
        const std::string& from = undo ? retype_->marked : retype_->typed;
        const std::string& to   = undo ? retype_->typed : retype_->marked;
        if (canSendBackspaces()) {
            performReplacement(from, to);
        } else {
            const auto length = static_cast<int>(ngosen::utf8::length(from));
            host_->deleteSurrounding(-length, static_cast<unsigned int>(length));
            commit(to);
        }
        if (undo)
            retype_.reset();
        else
            retype_->applied = true;
    }

    void TypingState::hideOffer() {
        host_->hideCandidates();
        host_->setStatus({});
        host_->refreshPanel();
    }

    void TypingState::dropRetype() {
        if (!retype_)
            return;
        if (!retype_->applied || retype_->atOnce)
            hideOffer();
        retype_.reset();
        retype_timer_.reset();
    }

    bool TypingState::handleRetypeKey(ngosen::KeyPress& keyEvent) {
        if (keyEvent.isRelease() || keyEvent.isModifier())
            return false;
        const uint32_t sym = keyEvent.sym();
        if (retype_->atOnce) {
            if (is_deleting_.load(std::memory_order_acquire))
                return false;
            if (sym == ngosen::key::Escape && !keyEvent.hasModifier()) {
                keyEvent.accept();
                if (retype_->applied)
                    applyRetype();
                else
                    dropRetype();
                return true;
            }
            dropRetype();
            return false;
        }
        if (sym == ngosen::key::Tab && !keyEvent.hasModifier()) {
            keyEvent.accept();
            // Tab again before the first retype reaches the app would delete the wrong text.
            if (!is_deleting_.load(std::memory_order_acquire))
                applyRetype();
            return true;
        }
        // Keys arriving mid-replacement are our own: backspaces an XIM client hands back, or the
        // Shift+Left that selects text in Chromium. They are not the user moving on.
        if (is_deleting_.load(std::memory_order_acquire))
            return false;
        if (sym == ngosen::key::Escape && !retype_->applied) {
            keyEvent.accept();
            dropRetype();
            return true;
        }
        dropRetype();
        return false;
    }

} // namespace ngosen
