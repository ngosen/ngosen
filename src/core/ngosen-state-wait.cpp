/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
// Sending backspaces and waiting for the app to apply them.
#include "ngosen-state.h"
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-utf8.h"
#include "ngosen-xtest.h"

#include <algorithm>
#include <string>

namespace ngosen {

    void TypingState::sendBackspaceKeys(int count) const {
        if (!host_->pressSystemKeys(count)) {
            NGOSEN_ERROR("Cannot send backspaces: XTEST is unavailable");
        }
    }

    bool TypingState::deletionLooksDone() const {
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            return false;
        }
        const std::string& t = s.text();
        // A snapshot identical to the one taken when the backspaces were sent cannot mean "done" by
        // content alone: when the app lags by a couple of keys, the stale snapshot looks exactly like
        // the finished state.
        if (!surr_wait_sent_snapshot_.empty() && t + "\x1f" + std::to_string(s.cursor()) == surr_wait_sent_snapshot_) {
            // Firefox may never send another state, so accept it after WaitSurroundingMinPerKeyMs per
            // backspace, as long as a plain sleep. The immediate check still rejects it.
            const auto waited  = (ngosen::monotonicUs() - surr_wait_started_at_) / 1000;
            const auto minimum = static_cast<uint64_t>(engine_->options().waitSurroundingMinPerKeyMs) * static_cast<uint64_t>(std::max(expected_backspaces_, 1));
            if (waited < minimum) {
                return false;
            }
        }
        auto it = t.begin();
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
            it = ngosen::utf8::nextChar(it, t.end());
        }
        const std::string before(t.begin(), it);
        auto              endsWith = [](const std::string& a, const std::string& b) { return a.size() >= b.size() && a.compare(a.size() - b.size(), b.size(), b) == 0; };
        // Stale or half done: some of the text to delete is still there. Qt reports after each backspace.
        for (auto d = surr_wait_deleted_.begin(); d != surr_wait_deleted_.end();) {
            d = ngosen::utf8::nextChar(d, surr_wait_deleted_.end());
            if (endsWith(before, surr_wait_prefix_ + std::string(surr_wait_deleted_.begin(), d)))
                return false;
        }
        // Messenger moves the cursor before it removes the text, so a snapshot can look done while
        // deleted chars still follow the cursor. Treat that as in progress (#267).
        if (!surr_wait_deleted_.empty() && it != t.end()) {
            const std::string charAfter(it, ngosen::utf8::nextChar(it, t.end()));
            std::string       passed = surr_wait_prefix_;
            for (auto d = surr_wait_deleted_.begin(); d != surr_wait_deleted_.end();) {
                const auto next = ngosen::utf8::nextChar(d, surr_wait_deleted_.end());
                if (charAfter == std::string(d, next) && endsWith(before, passed)) {
                    return false;
                }
                passed.append(d, next);
                d = next;
            }
        }
        return endsWith(before, surr_wait_prefix_);
    }

    bool TypingState::handleReplacementBackspace(ngosen::KeyPress& event, uint32_t currentSym, int sleepTime) {
        if (!is_deleting_.load()) {
            return false;
        }
        if (ngosen::key::isBackspace(currentSym)) {
            current_backspace_count_ += 1;
            if (current_backspace_count_ < expected_backspaces_) {
                return false; // Allow intermediate backspaces to reach the app to clear autofill/old text.
            }
            return waitForDeletion(&event, sleepTime);
        }
        return false;
    }

    // Waits until the app has applied the backspaces, then commits. `event` is the returning sentinel
    // backspace on the XTEST path, or null when the backspaces were forwarded.
    bool TypingState::waitForDeletion(ngosen::KeyPress* event, int sleepTime) {
        // Some apps (Konsole) declare surrounding text but always send it empty; nothing can match,
        // so use the sleeping path.
        const bool emptySnapshot = host_->surrounding().text().empty() || surr_silent_;
        const bool waitEvent     = engine_->options().waitSurroundingEvent;
        if (waitEvent && emptySnapshot) {
            NGOSEN_INFO(surr_silent_ ? "Surr wait skip: silent app" : "Surr wait skip: empty snapshot");
        }
        bool skipFrozenWait = false;
        if (waitEvent && !emptySnapshot && surr_frozen_) {
            const int probeEvery = std::max(engine_->options().waitSurroundingProbeEvery, 1);
            ++surr_frozen_probe_count_;
            if (surr_frozen_probe_count_ % probeEvery != 0) {
                skipFrozenWait = true;
            }
        }
        if (waitEvent && !emptySnapshot && !skipFrozenWait)
            return startSurroundingWait(event);
        return startTimedWait(event, sleepTime, skipFrozenWait);
    }

    bool TypingState::startSurroundingWait(ngosen::KeyPress* event) {
        // Sleeping blocks the single event loop, so no update could arrive. Return to the loop
        // and watch for updates from now on; a fresh watcher ignores the previous replacement's
        // late events. Keys typed meanwhile go to buffered_keys_.
        if (event != nullptr) {
            event->accept();
        }
        surr_wait_started_at_ = ngosen::monotonicUs();
        // After a timeout the app is lagging (Firefox) and its snapshot is stale: skip the
        // immediate check.
        if (surr_snapshot_trusted_ && deletionLooksDone()) {
            NGOSEN_INFO("Skip retry");
            deliverAfterSettle("immediate", false);
            return true;
        }
        surr_wait_pending_            = true;
        surr_wait_event_count_        = 0;
        surr_wait_saw_other_snapshot_ = false;
        surr_wait_watching_           = true;
        // Two timeouts in a row: this app does not update while deleting. Use the short timeout
        // until an event matches again.
        const int  timeoutMs = surr_timeout_streak_ >= 2 ? engine_->options().waitSurroundingShortMs : engine_->options().waitSurroundingTimeoutMs;
        const auto timeout   = static_cast<uint64_t>(timeoutMs) * 1000ULL;
        // Accuracy 0 means sd-event's default 250 ms slack, so pass 1 ms. Check once at the
        // threshold first: many apps report "done" before it and then go quiet.
        const auto threshold     = static_cast<uint64_t>(engine_->options().waitSurroundingMinPerKeyMs) * static_cast<uint64_t>(std::max(expected_backspaces_, 1)) * 1000ULL;
        const auto firstDeadline = threshold < timeout ? surr_wait_started_at_ + threshold : surr_wait_started_at_ + timeout;
        surr_wait_timer_         = host_->startTimer(firstDeadline, 1000, [this, timeout](ngosen::Timer& t) {
            if (!surr_wait_pending_ || surr_wait_timer_only_ || !is_deleting_.load()) {
                return false;
            }
            const auto waited = ngosen::monotonicUs() - surr_wait_started_at_;
            if (waited + 1000 < timeout) {
                if (deletionLooksDone()) {
                    deliverAfterSettle("threshold", true);
                    return false;
                }
                t.rearm(surr_wait_started_at_ + timeout);
                return true;
            }
            if (deferLaggingCommit("timeout"))
                return false;
            finishReplacement("timeout", true);
            return false;
        });
        return true;
    }

    bool TypingState::startTimedWait(ngosen::KeyPress* event, int sleepTime, bool skipFrozenWait) {
        // Frozen snapshot: sleep at least WaitSurroundingMinPerKeyMs x (N - 1) instead of, not on
        // top of, the normal sleep.
        const int perKeyMs = skipFrozenWait ? std::max(sleepTime, engine_->options().waitSurroundingMinPerKeyMs) : sleepTime;
        int       waitMs   = perKeyMs * (expected_backspaces_ - 1);
        // Validate surr cursor pos should match realtextLen after all BS applied
        const auto surr = host_->surrounding();
        if (skipFrozenWait) {
            NGOSEN_INFO("Skip retry (frozen)"); // retrying 3 x 2 ms is pointless on a frozen snapshot
        } else if (surr.isValid() && surr.cursor() == realtextLen.load(std::memory_order_acquire)) {
            NGOSEN_INFO("Skip retry");
        } else if (!host_->field().surroundingText) {
            // No surrounding text capability (gnome-terminal, Chromium on X11): retrying cannot help.
            NGOSEN_INFO("Skip retry (no surrounding capability)");
        } else {
            // Retry x3 (2 ms each) for apps whose snapshot is not valid yet. Use a timer, not
            // sleep_for, so the event loop can deliver a fresh snapshot.
            waitMs += 3 * 2;
        }
        if (event != nullptr) {
            event->accept(); // filter out the returning sentinel backspace
        }
        if (waitMs <= 0) {
            finishReplacement("immediate", false);
            return true;
        }
        // Wait on a timer rather than sleep_for, which would block the fcitx5 event loop. Keys
        // arriving meanwhile go to buffered_keys_ because is_deleting_ is set.
        surr_wait_pending_       = true;
        surr_wait_timer_only_    = true;
        surr_wait_focus_retries_ = 0;
        surr_wait_started_at_    = ngosen::monotonicUs();
        surr_wait_deliver_at_    = surr_wait_started_at_ + (static_cast<uint64_t>(waitMs) * 1000ULL);
        surr_wait_timer_         = host_->startTimer(surr_wait_deliver_at_, 1000, [this](ngosen::Timer& t) { return onTimedWaitTimer(t); });
        return true;
    }

    bool TypingState::onTimedWaitTimer(ngosen::Timer& t) {
        if (!surr_wait_pending_ || !surr_wait_timer_only_) {
            return false;
        }
        if (!is_deleting_.load()) { // the replacement was cancelled elsewhere (navigation key...)
            surr_wait_pending_    = false;
            surr_wait_timer_only_ = false;
            return false;
        }
        if (waitsForRedraw()) {
            t.rearm(ngosen::monotonicUs() + 2000);
            return true;
        }
        if (!host_->hasFocus()) {
            // Chromium X11 leaves and re-enters the field within ~0.3 ms; a commit in that gap is
            // lost. Give it a moment to come back.
            if (++surr_wait_focus_retries_ <= 5) {
                t.rearm(ngosen::monotonicUs() + 2000);
                return true;
            }
            // The user really switched windows: the old field can no longer take the text. Reset
            // only this field's state; is_deleting_ is shared and the new field may be replacing.
            NGOSEN_INFO("Timer: input context lost focus, dropping text");
            surr_wait_pending_       = false;
            surr_wait_timer_only_    = false;
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            return false;
        }
        finishReplacement("timer", true);
        return false;
    }

    bool TypingState::deferLaggingCommit(const char* reason, uint64_t settleUs) {
        if (!ngosen::surroundingTextLags(host_->field()))
            return false;
        // A report from before the forwarded keys were applied can look like the finished deletion.
        const uint64_t floorUs = surr_wait_started_at_ + ForwardWaitUs;
        const uint64_t nowUs   = ngosen::monotonicUs();
        if (nowUs >= floorUs && host_->hasFocus() && !waitsForRedraw())
            return false;
        noteWaitOutcome(reason);
        surr_wait_pending_       = true;
        surr_wait_timer_only_    = true;
        surr_wait_focus_retries_ = 0;
        surr_wait_deliver_at_    = std::max(floorUs, nowUs + std::max<uint64_t>(settleUs, 1000));
        settle_timer_            = host_->startTimer(surr_wait_deliver_at_, 1000, [this](ngosen::Timer& t) { return onTimedWaitTimer(t); });
        return true;
    }

    void TypingState::forwardBackspaces(int count) {
        for (int i = 0; i < count; ++i) {
            forwardTap(ngosen::EditKey::BackSpace);
        }
    }

    bool TypingState::canSendBackspaces() const {
        const Field field = host_->field();
        if (field.sendsNoKeys)
            return field.surroundingText || ngosen::erasesWithCommittedDel(field);
        return ngosen::forwardsBackspaces(field) || ngosen::erasesWithCommittedDel(field) || xtestAvailable() || ngosen::takesSentKeyEvents(field);
    }

    void TypingState::deferTimedCommit(uint64_t deliverAtUs) {
        if (surr_wait_timer_ && deliverAtUs > surr_wait_deliver_at_) {
            surr_wait_deliver_at_ = deliverAtUs;
            surr_wait_timer_->rearm(deliverAtUs);
        }
    }

    void TypingState::surroundingUpdated() {
        // A wait that starts while this report is handled sees only the next report.
        const bool        waiting    = surr_wait_watching_;
        const bool        overtyping = overtype_watching_;
        const std::string report     = host_->field().password ? "hidden" : ngosen::describeSurrounding(host_->surrounding());
        // A repeat that a wait acts on stays in the log for its timing.
        if (waiting || overtyping || key_report_pending_)
            engine_->recorder().add("app", report);
        else
            engine_->recorder().addSurrounding(report);
        if (host_->hasFocus()) {
            checkCursorJump();
            checkRetoneReport();
        }
        surr_silent_        = false;
        surr_silent_streak_ = 0;
        if (!(host_->surrounding() == reportAtKey_))
            keySinceReport_ = false;
        if (key_report_pending_) {
            // Calc answers our Shift with an empty field when the key went to a cell not being edited.
            const auto s = host_->surrounding();
            finishKeyReportWait(s.isValid() && !(ngosen::reportsFieldOnlyOnKey(host_->field()) && s.text().empty()), false);
        }
        if (waiting)
            onWaitSurroundingUpdated();
        if (overtyping)
            onOvertypeSurroundingUpdated();
    }

    constexpr uint64_t KeyReportWaitUs = 20000;

    // The app reported text before this key but not with it, or it reports the field only on a key.
    bool TypingState::waitsForKeyReport() const {
        return hasLastSurrounding_ && (!host_->field().surroundingText || ngosen::reportsFieldOnlyOnKey(host_->field()));
    }

    void TypingState::startKeyReportWait(const std::string& deletedPart, const std::string& addedPart) {
        key_report_key_     = *live_key_;
        key_report_deleted_ = deletedPart;
        key_report_added_   = addedPart;
        key_report_pending_ = true;
        is_deleting_.store(true, std::memory_order_release);
        if (ngosen::reportsFieldOnlyOnKey(host_->field())) {
            // The key itself never reaches the app, so give it one that types nothing.
            forwardTap(ngosen::EditKey::Shift);
        }
        key_report_timer_ = host_->startTimer(ngosen::monotonicUs() + KeyReportWaitUs, 0, [this](ngosen::Timer&) {
            finishKeyReportWait(true, true);
            return false;
        });
    }

    // A field with no text to edit means the key went to a cell that is not being edited, so the word
    // before it is gone.
    void TypingState::finishKeyReportWait(bool sameField, bool fromTimer) {
        if (!key_report_pending_)
            return;
        key_report_pending_ = false;
        if (!fromTimer)
            key_report_timer_.reset(); // never reset a timer from inside its own callback
        if (sameField) {
            deleteAndCommit(key_report_deleted_, key_report_added_);
            return;
        }
        NGOSEN_INFO("Key went to another field, starting a new word");
        pending_commit_string_.clear();
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        hasHistory_              = false;
        ResetEngine(bambooEngine_.handle());
        oldPreBuffer_.clear();
        is_deleting_.store(false, std::memory_order_release);
        buffered_keys_.insert(buffered_keys_.begin(), key_report_key_);
        replayBufferedKeys();
    }

    void TypingState::onWaitSurroundingUpdated() {
        if (!surr_wait_pending_ || surr_wait_timer_only_ || !is_deleting_.load()) {
            return;
        }
        // After a timeout, an early event is the app's stale buffer catching up, not the
        // finished deletion. Ignore events before WaitSurroundingMinPerKeyMs per backspace.
        const auto waitedUs  = ngosen::monotonicUs() - surr_wait_started_at_;
        const auto minimumUs = static_cast<uint64_t>(engine_->options().waitSurroundingMinPerKeyMs) * static_cast<uint64_t>(std::max(expected_backspaces_, 1)) * 1000ULL;
        {
            const auto current = host_->surrounding();
            ++surr_wait_event_count_;
            if (current.text() + "\x1f" + std::to_string(current.cursor()) != surr_wait_sent_snapshot_) {
                surr_wait_saw_other_snapshot_ = true;
            }
        }
        if (!surr_snapshot_trusted_ && waitedUs < minimumUs) {
            // Stale buffer catching up after a timeout: ignore.
        } else if (deletionLooksDone()) {
            deliverAfterSettle("event", false);
        }
        // Not done yet: keep waiting silently. Anything worth printing here is text the user
        // just typed, which must not go into the log.
    }

    void TypingState::onOvertypeSurroundingUpdated() {
        if (!overtype_pending_ || !is_deleting_.load()) {
            return;
        }
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            return;
        }
        const int selected = static_cast<int>(s.anchor()) - static_cast<int>(s.cursor());
        if (selected != overtype_char_count_ && selected != -overtype_char_count_) {
            return;
        }
        finishOvertype("selected", false);
    }
    // Some apps move the caret back and forth for a few ms after a key (VS Code's EditContext on
    // Wayland). Nobody clicks that soon after typing, so such moves are not clicks.
    constexpr uint64_t CaretSettleUs = 30000;
    // A browser reports the step into its suggestion right after the key; a click takes longer.
    constexpr uint64_t SuggestionStepUs = 100000;

    // Our own edits only touch the text before the cursor, even while their report is still on the way.
    bool TypingState::textAfterCursorChanged(const Surrounding& s) const {
        const std::u32string before = ngosen::utf8::decode(lastSurroundingText_);
        const std::u32string now    = ngosen::utf8::decode(s.text());
        return before.substr(std::min<size_t>(lastSurroundingCursor_, before.size())) != now.substr(std::min<size_t>(s.cursor(), now.size()));
    }

    // A quick switch to another field on the same context comes back like a bounce. The first settled
    // report after it tells which: the same field still has the word before the cursor.
    bool TypingState::lostWordAfterBounce(const Surrounding& s, bool settling) {
        if (!checkWordAfterBounce_ || is_deleting_.load(std::memory_order_acquire) || deferred_commit_pending_ || unreportedCommitLength_ > 0)
            return false;
        checkWordAfterBounce_ = false;
        if (settling || mode() != ngosen::Mode::Sen || oldPreBuffer_.empty() || s.cursor() != s.anchor())
            return false;
        const std::u32string word   = ngosen::utf8::decode(oldPreBuffer_);
        const std::u32string before = ngosen::utf8::decode(s.text()).substr(0, s.cursor());
        return before.size() < word.size() || before.compare(before.size() - word.size(), word.size(), word) != 0;
    }

    // Wayland apps report a click only as a cursor move; the IM gets no reset or mouse event.
    void TypingState::checkCursorJump() {
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            hasLastSurrounding_ = false;
            return;
        }
        const bool sameText = hasLastSurrounding_ && s.text() == lastSurroundingText_;
        bool       jumped   = sameText && (s.cursor() != lastSurroundingCursor_ || s.anchor() != lastSurroundingAnchor_);
        // Some editors (Lark in Firefox) report the cursor past our commit before the committed text.
        const bool echo = jumped && unreportedCommitLength_ > 0 && s.cursor() == s.anchor() && s.cursor() == lastSurroundingCursor_ + unreportedCommitLength_;
        // Typing the next letter of an inline suggestion keeps the text and steps over that letter.
        // Edge reports the suggestion selected; Firefox does not, so a click one letter on looks the same
        // and only the timing tells them apart.
        const uint64_t sinceInputUs   = ngosen::monotonicUs() - lastInputAtUs_;
        const bool     intoSuggestion = jumped && passed_key_ && s.cursor() == lastSurroundingCursor_ + 1 &&
            ngosen::utf8::decode(s.text())[lastSurroundingCursor_] == passed_key_->sym &&
            ((s.anchor() == s.cursor() && sinceInputUs < SuggestionStepUs) || (s.anchor() == lastSurroundingAnchor_ && s.anchor() > s.cursor()));
        const bool settling = sinceInputUs < CaretSettleUs;
        // Antigravity's editor on GNOME moves the caret one step back well after the key (#362).
        const bool caretSettling = jumped && s.cursor() == s.anchor() && sinceInputUs < SuggestionStepUs && s.cursor() + 1 == lastSurroundingCursor_;
        // A spreadsheet reports a click on another cell only as that cell's text with the cursor at its start.
        const bool lostWord = lostWordAfterBounce(s, settling);
        const bool newField =
            (hasLastSurrounding_ && !sameText && s.cursor() == 0 && s.anchor() == 0 && !oldPreBuffer_.empty() && (unreportedCommitLength_ == 0 || textAfterCursorChanged(s))) ||
            lostWord;
        // Firefox repeats the old state before the echo, so only a change ends the wait.
        if (jumped || !sameText)
            unreportedCommitLength_ = 0;
        // Calc reports the new cell empty just before the key we let through lands in it.
        // Our unreported commits do not matter here: in the same cell the key would follow them.
        const bool keyStartedCell = settling && passed_key_ && !sameText && s.text().empty() && !oldPreBuffer_.empty() && ngosen::reportsFieldOnlyOnKey(host_->field());
        // Our own Right presses carrying the cursor to the end of a word being retoned.
        const bool ownMove   = retoneMove_.has_value();
        jumped               = ((jumped && !echo && !intoSuggestion && !caretSettling && !ownMove) || newField) && !settling;
        lastSurroundingText_ = s.text();
        // Keep the real caret, so the app reporting it again is not a move either.
        if (!caretSettling) {
            lastSurroundingCursor_ = s.cursor();
            lastSurroundingAnchor_ = s.anchor();
        }
        hasLastSurrounding_ = true;
        if (keyStartedCell && !is_deleting_.load(std::memory_order_acquire)) {
            startWordWithKey(*passed_key_);
            return;
        }
        // Our own selection for overtyping moves the anchor too.
        if (!jumped || is_deleting_.load(std::memory_order_acquire))
            return;
        NGOSEN_INFO("Cursor moved without an edit");
        needEngineReset.store(true, std::memory_order_release);
        g_mouse_clicked.store(true, std::memory_order_release);
    }

    // The key is already in the app; only the engine has to forget the word before it.
    void TypingState::startWordWithKey(const KeyEntry& key) {
        NGOSEN_INFO("Key went to another cell, starting a new word");
        clearAllBuffers();
        if (EngineProcessKeyEvent(bambooEngine_.handle(), key.sym, key.state) == 0U)
            return;
        ngosen::UniqueCPtr<char> preeditC(EnginePullPreedit(bambooEngine_.handle()));
        oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
        hasHistory_   = true;
    }

    void TypingState::noteCommit(const std::string& text) {
        // fcitx reports our own commits too; commit() has recorded those already.
        if (committing_)
            return;
        unreportedCommitLength_ += ngosen::utf8::length(text);
        lastInputAtUs_ = ngosen::monotonicUs();
    }

    void TypingState::commit(const std::string& text) {
        noteCommit(text);
        // A field that rewrites what we commit never shows it; keep only the latest commits.
        if (mode() == ngosen::Mode::Sen && ngosen::appliesEarlyKeysFirst(host_->field()))
            commitNotSeen_ = commitNotSeen_.size() + text.size() > 64 ? text : commitNotSeen_ + text;
        committing_ = true;
        host_->commitText(text);
        committing_ = false;
    }

} // namespace ngosen
