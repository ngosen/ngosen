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
        if (!resume)
            is_deleting_.store(false);

        realMode = targetMode;
        if (bounce)
            NGOSEN_INFO("Focus bounce: keep word buffers");
        else
            clearAllBuffers();

        if (isFocusIn && host_->field().frontend == "dbus" && !s.isValid()) {
            NGOSEN_INFO("Skip clearAllBuffers");
        } else if (s.isValid() && !oldPreBuffer_.empty() && now - lastDeactivateTime_ >= FocusBounceMs) {
            clearAllBuffers();
        }
        if (!resume)
            is_deleting_.store(false);
        needEngineReset.store(false);
        return bounce;
    }

    void TypingState::deactivate(bool isFocusOut) {
        const bool surroundingValid = host_->surrounding().isValid();
        flushPendingReplacement(); // commit pending text into the field we are leaving
        lastDeactivateTime_ = nowMs();
        if (realMode == ngosen::Mode::Preedit && !isFocusOut) {
            commitBuffer();
            return;
        }
        if (isFocusOut && host_->field().frontend == "dbus" && !surroundingValid) {
            NGOSEN_INFO("Skip clearAllBuffers");
        } else if (surroundingValid && !oldPreBuffer_.empty()) {
            clearAllBuffers();
        }
        // A selection waiting to be typed over has no backspaces left, but is just as unfinished.
        if (realMode == ngosen::Mode::Sen && is_deleting_.load() && (expected_backspaces_ > 0 || overtype_pending_)) {
            deletionInterruptedAt_ = nowMs();
            NGOSEN_INFO("Replacement interrupted by focus out");
        } else {
            is_deleting_.store(false);
        }
        needEngineReset.store(false);
        host_->resetPanel();
        host_->refreshPanel();
        if (realMode == ngosen::Mode::Preedit || realMode == ngosen::Mode::Emoji)
            host_->refreshPreedit();
    }

} // namespace ngosen
