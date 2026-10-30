/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
// TypingState: replacing the typed word in the app, by backspaces or by selecting over it.
#include "ngosen-state.h"

#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-strings.h"
#include "ngosen-surrounding-count.h"
#include "ngosen-utf8.h"
#include "ngosen-xtest.h"

#include <cstddef>
#include <algorithm>
#include <string>
#include <vector>
#include <thread>

namespace ngosen {

    // Gives up on a selection whose Shift release never comes back.
    constexpr uint64_t XTestSelectTimeoutUs = 500000;

    namespace {
        const char* matchName(SurroundingDeletion::Match match) {
            switch (match) {
                case SurroundingDeletion::Match::Fresh: return "current";
                case SurroundingDeletion::Match::OldTail: return "earlier";
                case SurroundingDeletion::Match::None: break;
            }
            return "unrelated";
        }

        // Firefox's address bar sets no Url flag and reports its autofill suffix as unselected, so
        // recognise it by shape: one line, and only URL characters after the cursor.
        bool isUrlChar(uint32_t c) {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                return true;
            }
            switch (c) {
                case '-':
                case '.':
                case '_':
                case '~':
                case ':':
                case '/':
                case '?':
                case '#':
                case '[':
                case ']':
                case '@':
                case '!':
                case '$':
                case '&':
                case '\'':
                case '(':
                case ')':
                case '*':
                case '+':
                case ',':
                case ';':
                case '=':
                case '%': return true;
                default: return false;
            }
        }

        bool textAfterCursorLooksLikeUrl(const ngosen::Surrounding& s) {
            if (!s.isValid() || s.cursor() != s.anchor()) {
                return false;
            }
            const unsigned cursor = s.cursor();
            unsigned       i      = 0;
            size_t         after  = 0;
            for (char32_t c : ngosen::utf8::decode(s.text())) {
                if (c == U'\n') {
                    return false;
                }
                if (i >= cursor) {
                    if (!isUrlChar(c)) {
                        return false;
                    }
                    ++after;
                }
                ++i;
            }
            return after > 0;
        }
    } // namespace

    namespace {
        // The word being typed starts the field, so an extra backspace for a wrong autofill guess
        // deletes nothing. realtextLen is stale here after the address bar is cleared.
        bool onlyCurrentWordBeforeCursor(const ngosen::Surrounding& s, const std::string& buff) {
            if (!s.isValid() || buff.empty() || s.cursor() != ngosen::utf8::length(buff)) {
                return false;
            }
            const std::string& t = s.text();
            return t.size() > buff.size() && t.compare(0, buff.size(), buff) == 0;
        }
    } // namespace

    bool TypingState::isAutofillCertain(const ngosen::Surrounding& s) {
        if (!s.isValid() || oldPreBuffer_.empty()) {
            return false;
        }

        const std::u32string u32Text         = ngosen::utf8::decode(s.text());
        const std::u32string u32OldPreBuffer = ngosen::utf8::decode(oldPreBuffer_);

        const unsigned int   cursor  = s.cursor();
        const unsigned int   anchor  = s.anchor();
        const size_t         textLen = u32Text.length();

        // Fix that surrounding text is delay update
        const size_t buffLen    = u32OldPreBuffer.length();
        const size_t pb         = u32Text.find(u32OldPreBuffer);
        size_t       rangeStart = static_cast<size_t>(cursor) >= buffLen ? static_cast<size_t>(cursor) - buffLen : 0;
        const bool   sameprefix = pb != std::u32string::npos && pb >= rangeStart && pb <= static_cast<size_t>(cursor);

        // Detect browser autofill/autocomplete suggestions via selection.
        if (cursor != anchor) {
            unsigned int selectionStart = std::min(anchor, cursor);
            unsigned int selectionEnd   = std::max(anchor, cursor);

            // Only consider it browser autofill if the selection starts at the cursor
            // and extends to the end of the line (common address bar behavior).
            if (selectionStart >= cursor || (selectionStart < cursor && selectionEnd > cursor)) {
                if (!sameprefix)
                    return false;
                // If the selection contains a newline, it's likely a multiline editor (AI ghost text),
                // not a single-line URL/Search bar.
                size_t p = u32Text.find(U'\n', selectionStart);
                return p == std::u32string::npos || p >= static_cast<size_t>(selectionEnd);
            }
        }

        if (textLen == static_cast<size_t>(cursor)) {
            realtextLen.store(textLen, std::memory_order_release);
            return false;
        }

        // Heuristic: rapid text growth in a single-line context.
        // Applied only when no newline is present after the cursor to distinguish from AI text in editors.
        // Gecko/Firefox: if buffLen > textLen, surrounding text is stale (async update race)
        if (buffLen > textLen) {
            return false;
        }
        if (textLen > static_cast<size_t>(cursor) + 1 && cursor == realtextLen.load(std::memory_order_acquire) && u32Text.find(U'\n', cursor) == std::u32string::npos && sameprefix)
            return true;

        for (auto v = realtextLen.load(std::memory_order_acquire); v < cursor && !realtextLen.compare_exchange_weak(v, cursor, std::memory_order_acq_rel);)
            ;
        return false;
    }

    // The wait returns to the event loop, so focus can move while a commit is pending. deactivate()
    // clears is_deleting_ and a later timer would drop the text, so commit now.
    void TypingState::flushPendingReplacement() {
        if (!surr_wait_pending_) {
            return;
        }
        // Committing before a timer-only wait ends would land before the backspaces. The focus loss is
        // usually Chromium X11 bouncing focus, so let the timer commit.
        if (surr_wait_timer_only_) {
            return;
        }
        // Committing now would be lost; a field that empties itself refocuses within a few ms (#93),
        // and under GNOME an editor that redraws after our backspaces does too (#175).
        if (ngosen::dropsCommitsWithoutFocus(host_->field()) || ngosen::surroundingTextLags(host_->field())) {
            return;
        }
        finishReplacement("focus lost", false);
    }

    namespace {
        // The Messenger composer has "\n\n" right after the cursor, or is just "\n" when empty. Only
        // this field repaints over fresh text, so only it waits to settle.
        bool looksLikeMessengerComposer(const ngosen::Surrounding& s) {
            if (!s.isValid()) {
                return false;
            }
            const std::string& t = s.text();
            if (t == "\n") {
                return true;
            }
            auto it = t.begin();
            for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
                it = ngosen::utf8::nextChar(it, t.end());
            }
            return std::string(it, t.end()) == "\n\n";
        }

        // Facebook composers (message and post box) report text ending in "\n\n" wherever the cursor
        // is, or just "\n" when empty. Check the whole field so mid-text edits match too.
        bool looksLikeFacebookComposer(const ngosen::Surrounding& s) {
            if (!s.isValid()) {
                return false;
            }
            const std::string& t = s.text();
            return t == "\n" || (t.size() >= 2 && t.compare(t.size() - 2, 2, "\n\n") == 0);
        }

        // No space or newline before the cursor: the first word of the message is being typed.
        bool isFirstWordOfMessage(const ngosen::Surrounding& s) {
            const std::string& t   = s.text();
            auto               end = t.begin();
            for (unsigned int i = 0; i < s.cursor() && end != t.end(); ++i) {
                end = ngosen::utf8::nextChar(end, t.end());
            }
            return std::find_if(t.begin(), end, [](char c) { return c == ' ' || c == '\n'; }) == end;
        }
    } // namespace

    void TypingState::deliverAfterSettle(const char* reason, bool fromTimer) {
        const auto snapshot = host_->surrounding();
        int        settleMs = engine_->options().waitSurroundingSettleMs;
        if (settleMs <= 0 || !looksLikeMessengerComposer(snapshot)) {
            settleMs = 0;
        } else if (isFirstWordOfMessage(snapshot)) {
            // A just-emptied composer is still reloading its placeholder, so the first word waits longer.
            settleMs = std::max(settleMs, engine_->options().waitSurroundingSettleFirstWordMs);
        }
        // A composer drops a commit that lands right after the deletion, also under GNOME (#362).
        if (deferLaggingCommit(reason, static_cast<uint64_t>(settleMs) * 1000ULL))
            return;
        if (settleMs == 0) {
            finishReplacement(reason, fromTimer);
            return;
        }
        // Reuse the timer-only wait state: a key arriving mid-wait finishes the wait first, focus loss
        // leaves the commit to the timer, and the snapshot watcher and timeout stay quiet.
        surr_wait_pending_       = true; // the immediate path gets here without the flag set
        surr_wait_timer_only_    = true;
        surr_wait_focus_retries_ = 0;
        settle_reason_           = reason;
        surr_wait_deliver_at_    = ngosen::monotonicUs() + (static_cast<uint64_t>(settleMs) * 1000ULL);
        settle_timer_            = host_->startTimer(surr_wait_deliver_at_, 1000, [this](ngosen::Timer&) {
            if (!surr_wait_pending_ || !surr_wait_timer_only_ || !is_deleting_.load()) {
                return false;
            }
            finishReplacement(settle_reason_, false);
            return false;
        });
    }

    void TypingState::noteWaitOutcome(const char* reason) {
        const auto elapsedMs = (ngosen::monotonicUs() - surr_wait_started_at_) / 1000;
        NGOSEN_INFO("Surr wait " + std::string(reason) + " after " + std::to_string(elapsedMs) + " ms");
        if (std::string(reason) == "timeout") {
            surr_snapshot_trusted_ = false;
            ++surr_timeout_streak_;
            if (surr_wait_event_count_ == 0 && ngosen::surroundingTextLags(host_->field()) && ++surr_silent_streak_ >= 2 && !surr_silent_) {
                surr_silent_ = true;
                NGOSEN_INFO("Surr silent: use the timed wait");
            }
            // Frozen means at least two events, all identical to the send-time snapshot. A Firefox that
            // is merely lagging usually sends nothing or a snapshot that is still moving.
            if (surr_wait_event_count_ >= 2 && !surr_wait_saw_other_snapshot_ && surr_wait_sent_snapshot_fresh_) {
                if (++surr_frozen_streak_ >= 2 && !surr_frozen_) {
                    surr_frozen_             = true;
                    surr_frozen_probe_count_ = 0;
                    NGOSEN_INFO("Surr frozen: stop waiting");
                }
            } else {
                surr_frozen_streak_ = 0;
            }
        } else if (std::string(reason) == "event" || std::string(reason) == "threshold") {
            surr_snapshot_trusted_ = true;
            surr_timeout_streak_   = 0;
            surr_frozen_streak_    = 0;
            if (surr_frozen_) {
                surr_frozen_ = false;
                NGOSEN_INFO("Surr frozen: resume waiting");
            }
        }
    }

    void TypingState::finishReplacement(const char* reason, bool fromTimer) {
        noteWaitOutcome(reason);
        surr_wait_pending_    = false;
        surr_wait_timer_only_ = false;
        if (!fromTimer && surr_wait_timer_) {
            surr_wait_timer_.reset(); // never reset a timer from inside its own callback
        }
        // A previous replacement still waiting for its turn goes out first, keeping the text in order.
        flushDeferredCommit();
        // wlroots drops a commit sent while the app's report is still arriving, as answering an old state.
        // Focus loss is not inside a key event, and the field is going away, so it commits at once.
        const bool defer = (host_->field().frontend == "dbus" || std::string(reason) == "event") && std::string(reason) != "focus lost";
        if (defer) {
            deferred_commit_text_    = pending_commit_string_;
            deferred_commit_pending_ = true;
            deferred_commit_timer_   = host_->startTimer(ngosen::monotonicUs(), 0, [this](ngosen::Timer&) {
                flushDeferredCommit();
                return false; // never reset a timer from inside its own callback
            });
        }
        std::string text         = defer ? std::string() : std::move(pending_commit_string_);
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        pending_commit_string_.clear();
        is_deleting_.store(false);
        if (!defer) {
            replayBufferedKeys(std::move(text));
        }
    }

    void TypingState::flushDeferredCommit() {
        if (!deferred_commit_pending_) {
            return;
        }
        deferred_commit_pending_ = false;
        std::string text         = std::move(deferred_commit_text_);
        deferred_commit_text_.clear();
        replayBufferedKeys(std::move(text));
    }

    void TypingState::sendSelectKeys(int charCount) const {
        sendBackspaceKeys(-charCount);
    }

    void TypingState::selectAndOvertype(const std::string& addedPart, int charCount, bool viaXTest) {
        is_deleting_.store(true, std::memory_order_release);
        overtype_via_xtest_      = viaXTest;
        overtype_shift_released_ = false;
        pending_commit_string_   = addedPart;
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        overtype_char_count_     = charCount;
        overtype_pending_        = true;
        {
            const auto snapshot     = host_->surrounding();
            overtype_had_snapshot_  = snapshot.isValid();
            overtype_cursor_before_ = overtype_had_snapshot_ ? snapshot.cursor() : 0;
        }
        overtype_started_at_     = ngosen::monotonicUs();
        overtype_watching_       = true;
        const uint64_t timeoutUs = viaXTest ? XTestSelectTimeoutUs : 150000ULL;
        overtype_timer_          = host_->startTimer(overtype_started_at_ + timeoutUs, 1000, [this](ngosen::Timer&) {
            if (overtype_pending_ && is_deleting_.load()) {
                if (overtype_shift_released_) {
                    finishOvertype("selection settled", true);
                } else {
                    abandonOvertype();
                }
            }
            return false;
        });
        // forwardKey does not carry Shift into the selection, so press the keys like a real keyboard.
        sendSelectKeys(charCount);
        NGOSEN_INFO("Select " + std::to_string(charCount) + " chars");
    }

    // The field did not report the selection in time. Do not type over it: the cursor has moved and
    // the text would land in the wrong place. Move the cursor back and drop this replacement; the
    // user loses one tone mark and sees it immediately.
    void TypingState::abandonOvertype() {
        const auto snapshot     = host_->surrounding();
        int        rightPresses = 1; // a real but unreported selection collapses with one Right
        if (overtype_had_snapshot_ && snapshot.isValid() && snapshot.cursor() == snapshot.anchor() &&
            snapshot.cursor() + static_cast<unsigned int>(overtype_char_count_) == overtype_cursor_before_) {
            rightPresses = overtype_char_count_; // the field only moved the cursor, nothing selected
        }
        for (int i = 0; i < rightPresses; ++i) {
            forwardTap(ngosen::EditKey::Right);
        }
        NGOSEN_INFO("Overtype gave up after " + std::to_string((ngosen::monotonicUs() - overtype_started_at_) / 1000) + " ms, moved cursor right " + std::to_string(rightPresses));
        overtype_pending_   = false;
        overtype_via_xtest_ = false;
        pending_commit_string_.clear();
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        hasHistory_              = false;
        ResetEngine(bambooEngine_.handle());
        oldPreBuffer_.clear();
        is_deleting_.store(false);
        // Keys typed during the wait are user input; the cursor is back, so replay them.
        replayBufferedKeys();
    }

    void TypingState::finishOvertype(const char* reason, bool fromTimer) {
        const auto elapsedMs = (ngosen::monotonicUs() - overtype_started_at_) / 1000;
        NGOSEN_INFO("Overtype " + std::string(reason) + " after " + std::to_string(elapsedMs) + " ms");
        overtype_pending_   = false;
        overtype_via_xtest_ = false;
        if (!fromTimer && overtype_timer_) {
            overtype_timer_.reset(); // never reset a timer from inside its own callback
        }
        std::string text = std::move(pending_commit_string_);
        pending_commit_string_.clear();
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        is_deleting_.store(false);
        replayBufferedKeys(std::move(text));
    }

    void TypingState::performReplacement(const std::string& deletedPart, const std::string& addedPart) {
        NGOSEN_INFO("Perform replacement: " + logged(deletedPart) + " -> " + logged(addedPart)); //NOLINT
        // The deletion changes text we committed; the replacement's own commit is what to look for.
        commitNotSeen_.clear();
        current_backspace_count_ = 0;
        pending_commit_string_   = addedPart;
        expected_backspaces_     = static_cast<int>(ngosen::utf8::length(deletedPart));
        deletedChars_            = expected_backspaces_;
        recordSendSnapshot(deletedPart);
        trackEarlierWords(addedPart);
        if (live_key_ && waitsForKeyReport()) {
            startKeyReportWait(deletedPart, addedPart);
            return;
        }
        deleteAndCommit(deletedPart, addedPart);
    }

    void TypingState::deleteAndCommit(const std::string& deletedPart, const std::string& addedPart) {
        const auto surrounding = host_->surrounding();
        // A BackSpace in the Chromium address bar would only remove the selected autocompletion. XTEST,
        // unlike forwardKey, keeps Shift, so widen the selection over the old text and type over both.
        if (xtestAvailable() && ngosen::selectsOverAutocompletion(host_->field()) && !deletedPart.empty()) {
            selectAndOvertype(addedPart, static_cast<int>(ngosen::utf8::length(deletedPart)), true);
            return;
        }
        // Facebook composers only: other fields do not report a selection-only change, so the
        // overtype would time out and drop the tone mark.
        if (engine_->options().messengerSelectOvertype && !ngosen::forwardsBackspaces(host_->field()) && !host_->field().sendsNoKeys && looksLikeFacebookComposer(surrounding)) {
            selectAndOvertype(addedPart, static_cast<int>(ngosen::utf8::length(deletedPart)));
            return;
        }
        if (ngosen::erasesWithCommittedDel(host_->field())) {
            replaceThroughCommittedDel();
            return;
        }
        const bool isSurrText = deletesThroughSurrounding(surrounding);
        is_deleting_.store(true, std::memory_order_release);
        if (isSurrText) {
            replaceThroughSurrounding(surrounding);
            return;
        }
        if (ngosen::forwardsBackspaces(host_->field())) {
            replaceThroughForwardedKeys();
            return;
        }
        sendBackspaceKeys(expected_backspaces_);
        NGOSEN_INFO("Send " + std::to_string(expected_backspaces_) + " backspaces");
    }

    void TypingState::recordSendSnapshot(const std::string& deletedPart) {
        surr_wait_deleted_       = deletedPart;
        const auto snapshot      = host_->surrounding();
        surr_wait_sent_snapshot_ = reportKey(snapshot);
        surr_wait_prefix_        = (oldPreBuffer_.size() >= deletedPart.size()) ? oldPreBuffer_.substr(0, oldPreBuffer_.size() - deletedPart.size()) : std::string();
        // Only a fresh send-time snapshot (text before the cursor ends with prefix + deleted) may
        // count towards "frozen"; a lagging app sends a stale one.
        surr_wait_sent_snapshot_fresh_ = false;
        if (snapshot.isValid()) {
            const std::string before       = textBeforeCursor(snapshot);
            const std::string expected     = surr_wait_prefix_ + surr_wait_deleted_;
            surr_wait_sent_snapshot_fresh_ = before.size() >= expected.size() && before.compare(before.size() - expected.size(), expected.size(), expected) == 0;
        }
    }

    std::string TypingState::textBeforeCursor(const Surrounding& s) {
        const std::string& t  = s.text();
        auto               it = t.begin();
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i)
            it = utf8::nextChar(it, t.end());
        return std::string(t.begin(), it);
    }

    std::string TypingState::reportKey(const Surrounding& s) {
        return s.isValid() ? s.text() + "\x1f" + std::to_string(s.cursor()) : std::string();
    }

    std::string TypingState::loggedField(const Field& field) {
        return " (" + field.frontend + " " + field.program + ")";
    }

    std::vector<std::string> TypingState::surroundingPathFails(const Surrounding& surrounding, unsigned int& chars) const {
        const Field              field = host_->field();
        std::vector<std::string> fails;
        if (!surrSeenTextSinceFocus_)
            fails.emplace_back("no text since focus");
        chars = static_cast<unsigned int>(deletedChars_);
        if (!surrounding.isValid()) {
            fails.emplace_back("no copy");
        } else {
            const auto deletion = countSurroundingDeletion(textBeforeCursor(surrounding), surr_wait_prefix_, surr_wait_deleted_, earlierWords_);
            if (ngosen::countsDeletionOnCopy(field))
                chars = deletion.chars;
            if (surrounding.cursor() < chars)
                fails.emplace_back("cursor too close to the start");
            if (surrounding.cursor() != surrounding.anchor())
                fails.emplace_back("selection");
            if (deletion.match == SurroundingDeletion::Match::None)
                fails.emplace_back("copy shows another word");
        }
        // Some apps report nothing after a commit, or repeat the old text, until the next key; a deletion
        // counted on that copy would take characters already gone.
        if (unreportedCommitLength_ > 0)
            fails.emplace_back("copy older than the last commit");
        if (surroundingUntrusted())
            fails.emplace_back("marked untrusted");
        if (field.url || (ngosen::hidesAddressBarFlag(field) && textAfterCursorLooksLikeUrl(surrounding)))
            fails.emplace_back("address bar");
        // The backspace path waits for such a field to settle before it commits.
        if (fieldBounces_ > 0)
            fails.emplace_back("field refocuses after edits");
        return fails;
    }

    std::vector<std::string> TypingState::wordStartFails() const {
        const Field              field       = host_->field();
        const Surrounding        surrounding = host_->surrounding();
        std::vector<std::string> fails;
        if (!surrSeenTextSinceFocus_)
            fails.emplace_back("no text since focus");
        if (!surrounding.isValid())
            fails.emplace_back("no copy");
        else if (surrounding.cursor() != surrounding.anchor())
            fails.emplace_back("selection");
        if (surroundingUntrusted())
            fails.emplace_back("marked untrusted");
        // GNOME Shell stands in for every app over IBus, so the shape is all that tells an address bar apart.
        if (field.url || textAfterCursorLooksLikeUrl(surrounding))
            fails.emplace_back("address bar");
        return fails;
    }

    bool TypingState::keepsWordRaw(const std::string& deletedPart) {
        const Field field = host_->field();
        if (!field.preeditFallback || !deletesOnlyThroughSurrounding())
            return false;
        const Surrounding        surrounding = host_->surrounding();
        const std::string        prefix      = oldPreBuffer_.substr(0, oldPreBuffer_.size() - deletedPart.size());
        std::vector<std::string> fails;
        if (surroundingUntrusted())
            fails.emplace_back("marked untrusted");
        if (!surrounding.isValid()) {
            fails.emplace_back("no copy");
        } else {
            // The replacement has not been tracked yet, so the copy may still show the word before the last one.
            const auto deletion = countSurroundingDeletion(textBeforeCursor(surrounding), prefix, deletedPart, earlierWordsBeforeReplacement());
            if (deletion.match == SurroundingDeletion::Match::None)
                fails.emplace_back("copy shows another word");
            else if (surrounding.cursor() < deletion.chars)
                fails.emplace_back("cursor too close to the start");
        }
        if (fails.empty())
            return false;
        NGOSEN_INFO("Keep the word as typed" + loggedField(field) + ": " + joined(fails));
        rawWord_    = true;
        hasHistory_ = false;
        ResetEngine(bambooEngine_.handle());
        oldPreBuffer_.clear();
        return true;
    }

    bool TypingState::typesRaw(ngosen::KeyPress& keyEvent, uint32_t sym) {
        if (stopsReplacing()) {
            hasHistory_ = false;
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            keyEvent.passToApp();
            return true;
        }
        if (!rawWord_)
            return false;
        const std::u32string chars = ngosen::utf8::decode(host_->keyText(sym));
        // Telex types ư and ơ with the bracket keys, so they belong to the word too.
        const bool wordChar  = chars.size() == 1 && (isWordChar(chars[0]) || chars[0] == U'[' || chars[0] == U']');
        const bool shiftOnly = (keyEvent.states() & ~(ngosen::modifier::Shift | ngosen::modifier::Repeat)) == 0;
        if (shiftOnly && (wordChar || ngosen::key::isBackspace(sym))) {
            keyEvent.passToApp();
            return true;
        }
        rawWord_ = false;
        return false;
    }

    void TypingState::noteAutofill(const Surrounding& report) {
        const Field field = host_->field();
        if (!field.preeditFallback)
            return;
        const Surrounding previous = lastReport_;
        lastReport_                = report;
        // A key's own report can carry the suggestion too; only one that comes on its own is told apart.
        if (autofillMarked_ || oldPreBuffer_.empty() || keySinceReport_ || !previous.isValid() || !report.isValid())
            return;
        const std::string before = textBeforeCursor(report);
        if (before != textBeforeCursor(previous) || !isEndsWith(before, oldPreBuffer_))
            return;
        if (report.text().size() - before.size() <= previous.text().size() - before.size())
            return;
        autofillMarked_ = true;
        NGOSEN_INFO("Text appeared after the cursor without a key" + loggedField(field) + "; type the next words in the preedit");
    }

    std::string TypingState::joined(const std::vector<std::string>& parts) {
        std::string out;
        for (const auto& part : parts)
            out += (out.empty() ? "" : ", ") + part;
        return out.empty() ? "none" : out;
    }

    void TypingState::notePath(const char* path, const std::string& why) {
        if (lastPath_ != path)
            NGOSEN_INFO(std::string("Path changed to ") + path + loggedField(host_->field()) + ": " + why);
        lastPath_ = path;
    }

    bool TypingState::deletesThroughSurrounding(const ngosen::Surrounding& surrounding) {
        if (deletesOnlyThroughSurrounding()) {
            notePath("surrounding", "the field takes no keys");
            return true;
        }
        if (host_->field().surroundingText) {
            unsigned int chars = 0;
            const auto   fails = surroundingPathFails(surrounding, chars);
            if (ngosen::deletesThroughSurroundingFirst(host_->field())) {
                if (fails.empty()) {
                    notePath("surrounding", "the field reports this word");
                    return true;
                }
                notePath("backspaces", joined(fails));
            } else {
                NGOSEN_INFO("Surrounding path would delete " + std::to_string(chars) + loggedField(host_->field()) + "; fails: " + joined(fails));
            }
        }
        ++expected_backspaces_;
        // Sen skips the autofill guard except in address bars (#190): the Url flag on Chromium,
        // the autofill shape on Firefox.
        const bool isFirefoxAddressBar = ngosen::hidesAddressBarFlag(host_->field()) && textAfterCursorLooksLikeUrl(surrounding);
        const bool checkAutofill       = mode() != ngosen::Mode::Sen || host_->field().url || isFirefoxAddressBar;
        if (checkAutofill) {
            // Enable Autofill detection for all frontends (Wayland/IBus).
            // This fixes the "toôi" duplication bug in Chromium-based search bars.
            // The isAutofillCertain function has been optimized to differentiate
            // between browser autofill and AI ghost text.
            // isAutofillCertain runs first so its realtextLen update still happens.
            if (isAutofillCertain(surrounding) || (isFirefoxAddressBar && onlyCurrentWordBeforeCursor(surrounding, oldPreBuffer_))) {
                ++expected_backspaces_;
            }
        }
        return false;
    }

    std::vector<std::string> TypingState::earlierWordsBeforeReplacement() const {
        // Raw keys after a replacement extend the word it left; anything else starts a new word.
        // trackEarlierWords keeps the same rule.
        if (wordAfterReplacement_.empty() || !isStartsWith(oldPreBuffer_, wordAfterReplacement_))
            return {};
        std::vector<std::string> words = earlierWords_;
        words.push_back(wordBeforeReplacement_);
        return words;
    }

    void TypingState::trackEarlierWords(const std::string& addedPart) {
        if (!wordAfterReplacement_.empty() && isStartsWith(oldPreBuffer_, wordAfterReplacement_))
            earlierWords_.push_back(wordBeforeReplacement_);
        else
            earlierWords_.clear();
        wordBeforeReplacement_ = oldPreBuffer_;
        wordAfterReplacement_  = surr_wait_prefix_ + addedPart;
    }

    int TypingState::surroundingDeletionCount(const Surrounding& surrounding, std::string& retype) const {
        if (!ngosen::countsDeletionOnCopy(host_->field()) || !surrounding.isValid())
            return deletedChars_;
        const auto deletion = countSurroundingDeletion(textBeforeCursor(surrounding), surr_wait_prefix_, surr_wait_deleted_, earlierWords_);
        NGOSEN_INFO(std::string("Surrounding copy shows the ") + matchName(deletion.match) + " word: delete " + std::to_string(deletion.chars) + " of " +
                    std::to_string(deletedChars_));
        retype = deletion.retype;
        return static_cast<int>(deletion.chars);
    }

    void TypingState::replaceThroughSurrounding(const Surrounding& surrounding) {
        std::string retype;
        const int   count = surroundingDeletionCount(surrounding, retype);
        host_->deleteSurrounding(-count, static_cast<unsigned int>(count));
        NGOSEN_INFO("Delete using surrounding text");
        std::this_thread::sleep_for(std::chrono::milliseconds(engine_->options().surrDeleteSleepMs * count));
        const std::string text = retype + pending_commit_string_;
        if (!text.empty()) {
            commit(text);
            NGOSEN_INFO("Commit: " + logged(text));
            std::this_thread::sleep_for(std::chrono::milliseconds(engine_->options().surrCommitSleepMs * ngosen::utf8::length(text)));
        }
        openSurroundingCheck(surrounding, retype);
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        pending_commit_string_.clear();
        if (replaying_) {
            // The replay that led here resets the engine once this returns, so the keys after this
            // one wait for the next turn; new keys queue behind them meanwhile.
            replay_timer_ = host_->startTimer(monotonicUs(), 0, [this](Timer&) {
                is_deleting_.store(false);
                replayBufferedKeys();
                return false;
            });
            return;
        }
        is_deleting_.store(false);
        replayBufferedKeys();
    }

    void TypingState::replaceThroughCommittedDel() {
        NGOSEN_INFO("Erase " + std::to_string(expected_backspaces_) + " characters with DEL");
        std::string out(static_cast<size_t>(expected_backspaces_), '\x7f');
        out += pending_commit_string_;
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        pending_commit_string_.clear();
        replayBufferedKeys(std::move(out));
    }

    void TypingState::replaceThroughForwardedKeys() {
        // The XTEST count includes a sentinel that comes back to us; forwarded keys never do.
        const int count = expected_backspaces_ - 1;
        if (host_->field().frontend == "xim") {
            // We are inside the client's synchronous XIM request for the key that triggered this
            // replacement. Keys forwarded now reach the client before its reply, and libX11 may
            // hand them back to us unprocessed. Forward them once the reply has gone out.
            xim_forward_timer_ = host_->startTimer(ngosen::monotonicUs(), 0, [this, count](ngosen::Timer&) {
                if (is_deleting_.load()) {
                    forwardBackspaces(count);
                }
                return false;
            });
        } else {
            forwardBackspaces(count);
        }
        NGOSEN_INFO("Forward " + std::to_string(count) + " backspaces");
        waitForDeletion(nullptr, 4);
        // XIM, IBus and D-Bus clients queue forwarded keys, and on wayland_v2 they travel apart from the
        // commit, so the commit can overtake them.
        if (host_->field().frontend != "wayland" && surr_wait_timer_only_ && surr_wait_timer_) {
            deferTimedCommit(surr_wait_started_at_ + ForwardWaitUs);
        }
    }

    void TypingState::replayBufferedKeys(std::string committed) {
        // Under GNOME, mutter sends one text-input "done" per main-loop turn and clients keep only the
        // last commit_string before it, so back-to-back commits lose all but the last ("đ" then "i"
        // shows "i"). Send the replacement and the replayed keys as one commit.
        std::string out = std::move(committed);
        NGOSEN_INFO("Starting replay buffered keys");
        replaying_       = true;
        const auto guard = std::unique_ptr<bool, void (*)(bool*)>(&replaying_, [](bool* flag) { *flag = false; });
        if (buffered_keys_.empty()) {
            commitReplayed(out);
            return;
        }
        auto keys = std::move(buffered_keys_);
        buffered_keys_.clear();
        for (size_t i = 0; i < keys.size(); ++i) {
            if (replayKey(keys, i, out))
                return;
        }
        commitReplayed(out);
        NGOSEN_INFO("Replay buffered keys done");
    }

    void TypingState::commitReplayed(std::string& out) {
        if (!out.empty()) {
            commit(out);
            NGOSEN_INFO("Commit: " + logged(out));
            out.clear();
        }
    }

    void TypingState::replaceDuringReplay(const std::vector<KeyEntry>& keys, size_t i, std::string& out, const std::string& deletedPart, const std::string& addedPart) {
        // Re-buffer remaining keys for next replay cycle.
        for (size_t j = i + 1; j < keys.size(); ++j) {
            if (buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                buffered_keys_.push_back(keys[j]);
            }
        }
        commitReplayed(out);
        performReplacement(deletedPart, addedPart);
    }

    bool TypingState::replayKey(const std::vector<KeyEntry>& keys, size_t i, std::string& out) {
        auto        sym     = keys[i].sym;
        uint32_t    state   = keys[i].state;
        std::string keyUtf8 = host_->keyText(sym);
        if (keyUtf8.empty()) {
            return false;
        }

        bool processed = EngineProcessKeyEvent(bambooEngine_.handle(), sym, state) != 0U;

        auto commitF = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()));
        if (commitF && (*commitF.get() != 0)) {
            std::string deletedPart;
            std::string addedPart;
            ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, commitF.get(), deletedPart, addedPart);
            const bool replacing = !deletedPart.empty();
            if (replacing) {
                replaceDuringReplay(keys, i, out, deletedPart, addedPart);
            } else {
                out += addedPart;
            }
            hasHistory_ = false;
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            return replacing;
        }

        if (!processed) {
            out += keyUtf8;
            return false;
        }

        hasHistory_ = true;
        realtextLen.fetch_add(1, std::memory_order_acq_rel);

        ngosen::UniqueCPtr<char> preeditC(EnginePullPreedit(bambooEngine_.handle()));
        std::string              preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

        std::string              deletedPart;
        std::string              addedPart;
        if (ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) == 0)
            return false;
        if (deletedPart.empty()) {
            if (!addedPart.empty()) {
                out += addedPart;
                oldPreBuffer_ = preeditStr;
            }
            return false;
        }
        if (!canSendBackspaces()) {
            out += keyUtf8;
            return false;
        }
        if (is_deleting_.load()) {
            is_deleting_.store(false, std::memory_order_release);
        }
        replaceDuringReplay(keys, i, out, deletedPart, addedPart);
        oldPreBuffer_ = preeditStr;
        return true;
    }

} // namespace ngosen
