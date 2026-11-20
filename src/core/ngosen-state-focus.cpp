/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-clock.h"
#include "ngosen-globals.h"
#include "ngosen-log.h"
#include "ngosen-state.h"

namespace ngosen {

    namespace {
        // Focus that comes back sooner than this left only for a moment (Chromium on X11 bounces it).
        constexpr int64_t FocusBounceMs = 100;
        // A field that bounces again and again redraws its editor (Draft.js in Chrome on GNOME) and
        // drops a commit that arrives before the redraw our backspaces cause (#175), so the commit
        // waits until those bounces have come and gone, but no longer than RedrawWaitMs.
        constexpr int64_t RedrawSettleMs = 8;
        constexpr int64_t RedrawWaitMs   = 100;

        int64_t           nowMs() {
            return static_cast<int64_t>(ngosen::monotonicUs() / 1000);
        }
    } // namespace

    bool TypingState::activate(ngosen::Mode targetMode, bool isFocusIn) {
        const int64_t     now    = nowMs();
        const Surrounding s      = host_->surrounding();
        const bool        bounce = targetMode == ngosen::Mode::Sen && lastDeactivateTime_ > 0 && now - lastDeactivateTime_ < FocusBounceMs;
        const bool        resume = bounce && deletionInterruptedAt_ > 0 && is_deleting_.load();
        deletionInterruptedAt_   = 0;
        echoKeyAtUs_             = 0; // a report of another field would end it
        if (!resume)
            dropWaitsAfterFocusMove();
        // A page may move focus into its text field after the user selected text elsewhere on it.
        // A bounce keeps it: WPS opens a window on a double click and takes focus straight back.
        if (isFocusIn && !bounce)
            forgetSelection();
        if (!resume)
            is_deleting_.store(false);

        realMode = targetMode;
        if (bounce) {
            NGOSEN_INFO("Focus bounce: keep word buffers");
            checkWordAfterBounce_ = true;
            wordAtBounce_         = oldPreBuffer_;
            // Only a bounce right after a key or our own edit tells that the field redraws.
            const bool inWait = now - static_cast<int64_t>(surr_wait_.startedAt / 1000) < RedrawWaitMs;
            if (inWait || now - static_cast<int64_t>(lastInputAtUs_ / 1000) < RedrawWaitMs) {
                ++fieldBounces_;
                if (!lastKeyLeftText_)
                    ++redrawBounces_;
            }
            if (inWait) {
                waitBounces_   = bouncedInWait_ == surr_wait_.startedAt ? waitBounces_ + 1 : 1;
                bouncedInWait_ = surr_wait_.startedAt;
            }
            lastBounceAt_ = now;
        } else {
            clearAllBuffers();
            fieldBounces_         = 0;
            redrawBounces_        = 0;
            lastKeyLeftText_      = false;
            checkWordAfterBounce_ = false;
            wordAtBounce_.clear();
            emojiMuted_ = false;
            if (isFocusIn) {
                // A GNOME app's new context gets a copy of the previous window's text before focus-in, so
                // that copy is not text this field showed.
                sinceFocus_ = FieldHistory{};
                xtermText_.clear();
                // GTK sends its text again only when it changes, so the copy at focus-in counts as a report.
                sinceFocus_.reported    = s.isValid();
                sinceFocus_.keysAtFocus = keyCount_;
                // These also start over at other times, so they stay out of FieldHistory.
                surrCheck_.reset();
                surr_check_timer_.reset();
                editLeft_.reset();
                surrUnanswered_  = 0;
                copyWaitsRunOut_ = 0;
                // A new field starts a word even without a focus-out reset, which a reopened browser never sends.
                afterWordEnd_    = true;
                typedSinceFocus_ = false;
            }
        }

        // A click into another field of the same page looks like a bounce, and that field may keep its own text.
        if (isFocusIn) {
            sinceFocus_.showedBackspace = false;
            sinceFocus_.lastPath.clear();
            keysAtFocusIn_ = keyCount_;
            // The release of a cell's first key comes after the cell editor gets focus.
            unseenPress_.reset();
            // Firefox on KDE focuses the next cell's editor before it shows the letter; a field that already
            // held it shows no change.
            if (cellMove_)
                cellMove_->second = s.isValid() ? s.text() : std::string();
        }

        if (isFocusIn && host_->field().frontend == "dbus" && !s.isValid()) {
            NGOSEN_INFO("Skip clearAllBuffers");
        } else if (s.isValid() && !oldPreBuffer_.empty() && now - lastDeactivateTime_ >= FocusBounceMs) {
            clearAllBuffers();
        }
        if (!resume)
            is_deleting_.store(false);
        needEngineReset.store(false);
        jumpedFrom_.reset();
        retype_.reset();
        if (isFocusIn)
            forgetKeysWhileOff();
        return bounce;
    }

    void TypingState::deactivate(bool isFocusOut) {
        lockedWord_.reset();
        enter_after_replacement_ = false; // an Enter would land in the next field
        echoKeyAtUs_             = 0;
        flushPendingReplacement(); // commit pending text into the field we are leaving
        if (composedWait_) {
            // Its keys stay held, as a replacement's do.
            flushComposedCommit();
            composedWait_ = false;
        }
        if (isFocusOut) {
            rememberLeftField();
            keysAtFocusOut_ = keyCount_;
            commitNotSeen_.clear();
            overtype_.focusBounced          = overtype_.pending;
            field_keeps_focus_on_selection_ = false;
        }
        lastDeactivateTime_ = nowMs();
        if (mode() == ngosen::Mode::Preedit && !isFocusOut) {
            commitBuffer();
            return;
        }
        // The word stays until activate() knows whether focus only bounced.
        noteReplacementAtFocusOut();
        needEngineReset.store(false);
        jumpedFrom_.reset();
        retype_.reset();
        emojiOffer_.reset();
        emojiRecheck_.reset();
        dropRetoneKey();
        // Keys typed while another input method is active never reach us to take the selection.
        // On focus out, activate() decides once it knows whether focus only bounced.
        if (!isFocusOut)
            forgetSelection();
        host_->resetPanel();
        host_->refreshPanel();
        if (mode() == ngosen::Mode::Preedit)
            host_->refreshPreedit();
    }

    void TypingState::dropWaitsAfterFocusMove() {
        if (key_report_.pending) {
            key_report_.timer.reset();
            dropReplacementAfterFocusLoss();
        }
        if (split_edit_.pending) {
            split_edit_.pending = false;
            split_edit_.timer.reset();
            dropReplacementAfterFocusLoss();
        }
        if (tail_clear_.pending) {
            tail_clear_.pending = false;
            tail_clear_.timer.reset();
            dropReplacementAfterFocusLoss();
        }
    }

    // A selection waiting to be typed over has no backspaces left, but is just as unfinished.
    void TypingState::noteReplacementAtFocusOut() {
        if (mode() == ngosen::Mode::Sen && is_deleting_.load() && (expected_backspaces_ > 0 || overtype_.pending)) {
            deletionInterruptedAt_ = nowMs();
            NGOSEN_INFO("Replacement interrupted by focus out");
        } else {
            is_deleting_.store(false);
        }
    }

    bool TypingState::waitsForRedraw() const {
        const int64_t now     = nowMs();
        const auto    started = static_cast<int64_t>(surr_wait_.startedAt / 1000);
        if (redrawBounces_ < 2 || now - started >= RedrawWaitMs)
            return false;
        // Each deleted character brings its own redraw.
        const int bounces = bouncedInWait_ == surr_wait_.startedAt ? waitBounces_ : 0;
        return bounces < deletedChars_ || now - lastBounceAt_ < RedrawSettleMs || !host_->hasFocus();
    }

} // namespace ngosen
