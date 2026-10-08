/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
// LotusState: sending backspaces and waiting for the app to apply them.
#include "ngosen-state.h"
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-utf8.h"
#include "ngosen-xtest.h"

#include <algorithm>
#include <string>

namespace fcitx {

    void LotusState::sendBackspaceKeys(int count) const {
        if (!host_->pressSystemKeys(count)) {
            NGOSEN_ERROR("Cannot send backspaces: XTEST is unavailable");
        }
    }

    bool LotusState::deletionLooksDone() const {
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
        if (!surr_wait_deleted_.empty() && endsWith(before, surr_wait_prefix_ + surr_wait_deleted_)) {
            return false; // stale snapshot: the text to delete is still there
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

    bool LotusState::handleUInputKeyPress(ngosen::KeyPress& event, uint32_t currentSym, int sleepTime) {
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
    bool LotusState::waitForDeletion(ngosen::KeyPress* event, int sleepTime) {
        // Some apps (Konsole) declare surrounding text but always send it empty; nothing can match,
        // so use the sleeping path.
        const bool emptySnapshot = host_->surrounding().text().empty();
        // GNOME Shell relays surrounding text late and one step behind (or not at all), so an
        // event-driven wait times out and commits ahead of the backspaces. Use the timed path there.
        const bool waitEvent = engine_->options().waitSurroundingEvent && !ngosen::surroundingTextLags(host_->field());
        if (waitEvent && emptySnapshot) {
            NGOSEN_INFO("Surr wait skip: empty snapshot");
        }
        bool skipFrozenWait = false;
        if (waitEvent && !emptySnapshot && surr_frozen_) {
            const int probeEvery = std::max(engine_->options().waitSurroundingProbeEvery, 1);
            ++surr_frozen_probe_count_;
            if (surr_frozen_probe_count_ % probeEvery != 0) {
                skipFrozenWait = true;
            }
        }
        if (waitEvent && !emptySnapshot && !skipFrozenWait) {
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
                finishReplacement("timeout", true);
                return false;
            });
            return true;
        }
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
        surr_wait_timer_         = host_->startTimer(surr_wait_deliver_at_, 1000, [this](ngosen::Timer& t) {
            if (!surr_wait_pending_ || !surr_wait_timer_only_) {
                return false;
            }
            if (!is_deleting_.load()) { // the replacement was cancelled elsewhere (navigation key...)
                surr_wait_pending_    = false;
                surr_wait_timer_only_ = false;
                return false;
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
        });
        return true;
    }

    void LotusState::forwardBackspaces(int count) {
        for (int i = 0; i < count; ++i) {
            host_->forwardKey(ngosen::EditKey::BackSpace, false);
            host_->forwardKey(ngosen::EditKey::BackSpace, true);
        }
    }

    bool LotusState::canSendBackspaces() const {
        return ngosen::forwardsBackspaces(host_->field()) || xtestAvailable();
    }

    void LotusState::deferTimedCommit(uint64_t deliverAtUs) {
        if (surr_wait_timer_ && deliverAtUs > surr_wait_deliver_at_) {
            surr_wait_deliver_at_ = deliverAtUs;
            surr_wait_timer_->rearm(deliverAtUs);
        }
    }

    void LotusState::surroundingUpdated() {
        // A wait that starts while this report is handled sees only the next report.
        const bool waiting    = surr_wait_watching_;
        const bool overtyping = overtype_watching_;
        if (host_->hasFocus())
            checkCursorJump();
        if (waiting)
            onWaitSurroundingUpdated();
        if (overtyping)
            onOvertypeSurroundingUpdated();
    }

    void LotusState::onWaitSurroundingUpdated() {
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

    void LotusState::onOvertypeSurroundingUpdated() {
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

    // Wayland apps report a click only as a cursor move; the IM gets no reset or mouse event.
    void LotusState::checkCursorJump() {
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            hasLastSurrounding_ = false;
            return;
        }
        const bool sameText = hasLastSurrounding_ && s.text() == lastSurroundingText_;
        bool       jumped   = sameText && (s.cursor() != lastSurroundingCursor_ || s.anchor() != lastSurroundingAnchor_);
        // Some editors (Lark in Firefox) report the cursor past our commit before the committed text.
        const bool echo = jumped && unreportedCommitLength_ > 0 && s.cursor() == s.anchor() && s.cursor() == lastSurroundingCursor_ + unreportedCommitLength_;
        // Firefox repeats the old state before the echo, so only a change ends the wait.
        if (jumped || !sameText)
            unreportedCommitLength_ = 0;
        const bool settling    = ngosen::monotonicUs() - lastInputAtUs_ < CaretSettleUs;
        jumped                 = jumped && !echo && !settling;
        lastSurroundingText_   = s.text();
        lastSurroundingCursor_ = s.cursor();
        lastSurroundingAnchor_ = s.anchor();
        hasLastSurrounding_    = true;
        // Our own selection for overtyping moves the anchor too.
        if (!jumped || is_deleting_.load(std::memory_order_acquire))
            return;
        NGOSEN_INFO("Cursor moved without an edit");
        needEngineReset.store(true, std::memory_order_release);
        g_mouse_clicked.store(true, std::memory_order_release);
    }

    void LotusState::noteCommit(const std::string& text) {
        unreportedCommitLength_ += ngosen::utf8::length(text);
        lastInputAtUs_ = ngosen::monotonicUs();
    }

} // namespace fcitx
