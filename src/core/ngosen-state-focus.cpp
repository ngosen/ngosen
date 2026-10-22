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
        const bool        quick  = lastDeactivateTime_ > 0 && now - lastDeactivateTime_ < FocusBounceMs;
        const bool        bounce = targetMode == ngosen::Mode::Sen && quick;
        const bool        resume = bounce && deletionInterruptedAt_ > 0 && is_deleting_.load();
        deletionInterruptedAt_   = 0;
        if (!resume)
            is_deleting_.store(false);
        // Chromium editors such as Quill redraw after a deletion; the Preedit word outlives that bounce.
        ngosen::UniqueCPtr<char> open(bambooEngine_ ? EnginePullPreedit(bambooEngine_.handle()) : nullptr);
        const bool               openWord = open && *open.get() != 0;
        const bool               keepWord = quick && targetMode == ngosen::Mode::Preedit && (openWord || !reopenedWord_.empty());
        if (!reopenedWord_.empty() && !keepWord) {
            reopenUnsent_     = false;
            reopenBackspaces_ = 0;
            reopenedWord_.clear();
            reopenTimer_.reset();
        }

        realMode = targetMode;
        if (bounce) {
            NGOSEN_INFO("Focus bounce: keep word buffers");
            checkWordAfterBounce_ = true;
            // Only a bounce right after a key or our own edit tells that the field redraws.
            const bool inWait = now - static_cast<int64_t>(surr_wait_started_at_ / 1000) < RedrawWaitMs;
            if (inWait || now - static_cast<int64_t>(lastInputAtUs_ / 1000) < RedrawWaitMs)
                ++fieldBounces_;
            if (inWait) {
                waitBounces_   = bouncedInWait_ == surr_wait_started_at_ ? waitBounces_ + 1 : 1;
                bouncedInWait_ = surr_wait_started_at_;
            }
            lastBounceAt_ = now;
        } else if (!keepWord) {
            clearAllBuffers();
            fieldBounces_         = 0;
            checkWordAfterBounce_ = false;
        }

        if (isFocusIn && host_->field().frontend == "dbus" && !s.isValid()) {
            NGOSEN_INFO("Skip clearAllBuffers");
        } else if (s.isValid() && !oldPreBuffer_.empty() && now - lastDeactivateTime_ >= FocusBounceMs && !keepWord) {
            clearAllBuffers();
        }
        if (!resume)
            is_deleting_.store(false);
        needEngineReset.store(false);
        retype_.reset();
        if (isFocusIn)
            forgetKeysWhileOff();
        if (keepWord && openWord && reopenedWord_.empty()) {
            host_->showPreedit(open.get(), false);
            host_->refreshPreedit();
        }
        return bounce;
    }

    void TypingState::deactivate(bool isFocusOut) {
        flushPendingReplacement(); // commit pending text into the field we are leaving
        lastDeactivateTime_ = nowMs();
        if (realMode == ngosen::Mode::Preedit && !isFocusOut) {
            commitBuffer();
            return;
        }
        // The word stays until activate() knows whether focus only bounced.
        // A selection waiting to be typed over has no backspaces left, but is just as unfinished.
        if (realMode == ngosen::Mode::Sen && is_deleting_.load() && (expected_backspaces_ > 0 || overtype_pending_)) {
            deletionInterruptedAt_ = nowMs();
            NGOSEN_INFO("Replacement interrupted by focus out");
        } else {
            is_deleting_.store(false);
        }
        needEngineReset.store(false);
        retype_.reset();
        host_->resetPanel();
        host_->refreshPanel();
        if (realMode == ngosen::Mode::Preedit || realMode == ngosen::Mode::Emoji)
            host_->refreshPreedit();
    }

    bool TypingState::waitsForRedraw() const {
        const int64_t now     = nowMs();
        const auto    started = static_cast<int64_t>(surr_wait_started_at_ / 1000);
        if (fieldBounces_ < 2 || now - started >= RedrawWaitMs)
            return false;
        // Each deleted character brings its own redraw.
        const int bounces = bouncedInWait_ == surr_wait_started_at_ ? waitBounces_ : 0;
        return bounces < deletedChars_ || now - lastBounceAt_ < RedrawSettleMs || !host_->hasFocus();
    }

} // namespace ngosen
