/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: offering to retype, with marks, what was typed in English.
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <string>

namespace ngosen {

    namespace {
        // Enough for a sentence; older text is dropped a word at a time.
        constexpr size_t MaxTypedWhileOff = 200;

        bool             isPrintableAscii(uint32_t sym) {
            return sym >= 0x20 && sym <= 0x7e;
        }

        // Shift only picks the character, so it does not count.
        bool isShortcut(const ngosen::KeyPress& key) {
            return (key.states() & (ngosen::modifier::Ctrl | ngosen::modifier::Alt | ngosen::modifier::Super)) != 0U;
        }

        bool isWordKey(const std::string& text) {
            const std::u32string chars = ngosen::utf8::decode(text);
            return chars.size() == 1 && isWordChar(chars[0]);
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
        const std::string before(t.begin(), it);
        return before.size() >= typed.size() && before.compare(before.size() - typed.size(), typed.size(), typed) == 0;
    }

    void TypingState::offerRetype() {
        retype_.reset();
        // Still English: keep the text for the toggle key.
        if (realMode != ngosen::Mode::Sen && realMode != ngosen::Mode::Preedit)
            return;
        const std::string typed = std::move(typedWhileOff_);
        typedWhileOff_.clear();
        if (!bambooEngine_ || !engine_->options().retypeAfterSwitch || typed.empty())
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
            retype_ = Retype{.typed = typed, .marked = marked, .atOnce = true};
            applyRetypeSoon(host_->translate("Tab or Esc: put back what was typed"));
            return;
        }
        NGOSEN_INFO("Offer retype: " + typed + " -> " + marked);
        retype_ = Retype{.typed = typed, .marked = marked};
        host_->showCandidates({typed + " → " + marked}, 1, [this](size_t) {
            if (retype_ && !retype_->applied)
                acceptRetype(host_->translate("Tab or Esc: put back what was typed"));
        });
        host_->setStatus(host_->translate("Enter: retype in Vietnamese, Tab or Esc: keep"));
        host_->refreshPanel();
    }

    void TypingState::applyRetype() {
        if (!retype_)
            return;
        hideOffer();
        const bool undo = retype_->applied;
        replaceRetyped(undo ? retype_->marked : retype_->typed, undo ? retype_->typed : retype_->marked);
        if (undo)
            retype_.reset();
        else
            retype_->applied = true;
    }

    void TypingState::replaceRetyped(const std::string& from, const std::string& to) {
        if (canSendBackspaces()) {
            performReplacement(from, to);
        } else {
            const auto length = static_cast<int>(ngosen::utf8::length(from));
            host_->deleteSurrounding(-length, static_cast<unsigned int>(length));
            commit(to);
        }
    }

    void TypingState::acceptRetype(const std::string& hint) {
        applyRetype();
        // Undone like a retype at once: only Tab or Esc takes it back, and the next Enter reaches the app.
        retype_->atOnce = true;
        host_->setStatus(hint);
        host_->refreshPanel();
    }

    void TypingState::applyRetypeSoon(std::string hint) {
        // Not from inside the key or report being handled, which the app may still be waiting on.
        retype_timer_ = host_->startTimer(ngosen::monotonicUs(), 0, [this, hint = std::move(hint)](ngosen::Timer&) {
            if (retype_ && retype_->atOnce && !retype_->applied)
                acceptRetype(hint);
            return false;
        });
    }

    void TypingState::reopenTypedKey() {
        // The field still shows the retoned word, so the next key replaces it with the key and itself.
        EngineRebuildFromText(bambooEngine_.handle(), retype_->typed.c_str());
        oldPreBuffer_ = retype_->marked;
        dropRetype();
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
        // The click moved the cursor away from the text the offer would replace.
        if (g_mouse_clicked.load(std::memory_order_acquire) && !is_deleting_.load(std::memory_order_acquire)) {
            dropRetype();
            return false;
        }
        const uint32_t sym = keyEvent.sym();
        if (retype_->atOnce) {
            // Before the deletion check: a letter is the user's even while the retone is on its way.
            if (retype_->retone && retype_->applied && !isShortcut(keyEvent) && isWordKey(host_->keyText(sym)))
                return handleKeyAfterRetone(keyEvent);
            if (is_deleting_.load(std::memory_order_acquire))
                return false;
            if ((sym == ngosen::key::Escape || sym == ngosen::key::Tab) && !keyEvent.hasModifier()) {
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
        // Keys arriving mid-replacement are our own: backspaces an XIM client hands back, or the
        // Shift+Left that selects text in Chromium. They are not the user moving on.
        if (is_deleting_.load(std::memory_order_acquire))
            return false;
        const bool plain = !keyEvent.hasModifier();
        if ((sym == ngosen::key::Return || sym == ngosen::key::KP_Enter) && plain) {
            keyEvent.accept();
            acceptRetype(host_->translate("Tab or Esc: put back what was typed"));
            return true;
        }
        if ((sym == ngosen::key::Tab || sym == ngosen::key::Escape) && plain) {
            keyEvent.accept();
            dropRetype();
            return true;
        }
        dropRetype();
        return false;
    }

} // namespace ngosen
