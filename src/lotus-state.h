/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-state.h
 * @brief Input context state management for fcitx5-lotus.
 */

#ifndef _FCITX5_LOTUS_STATE_H_
#define _FCITX5_LOTUS_STATE_H_

#include "lotus.h"
#include "emoji-entry.h"
#include "lotus-utils.h"
#include "ngosen-engine-resources.h"
#include "ngosen-host.h"
#include "ngosen-key.h"

#include <cstddef>
#include <fcitx-utils/misc.h>
#include <fcitx/inputcontext.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/handlertable.h>
#include <fcitx/instance.h>
#include <fcitx/event.h>
#include <memory>

struct EmojiEntry;

namespace fcitx {
    class LotusEngine;
    class SurroundingText;

    /**
     * @brief Per-input-context state for Lotus input method.
     *
     * Manages the input state, buffers, and mode-specific handling for each input context.
     */
    class LotusState final : public InputContextProperty {
      public:
        /**
         * @brief Constructs a new state instance.
         * @param engine What all fields share through the engine.
         * @param host The input field this state types into.
         */
        LotusState(ngosen::EngineResources* engine, std::unique_ptr<ngosen::Host> host);

        /**
         * @brief Initializes the bamboo engine for this state.
         */
        void setEngine();

        /**
         * @brief Applies current options to the engine.
         */
        void setOption();

        /**
         * @brief Main key event handler.
         * @param keyEvent The key event to process.
         */
        void keyEvent(ngosen::KeyPress& keyEvent);

        /**
         * @brief Resets the input state.
         * @param isFocusOut If true, indicates the reset is due to a focus-out event, which may trigger committing the preedit text.
         */
        void reset(bool isFocusOut = false);

        // Called for every surrounding text report of this input context.
        void surroundingUpdated();

        /**
         * @brief Treats a cursor move within unchanged surrounding text as a mouse click.
         */
        void checkCursorJump();

        /**
         * @brief Records text sent to the app, so its cursor moving past it is not taken for a click.
         */
        void noteCommit(const std::string& text);

        /**
         * @brief Commits the current buffer.
         */
        void commitBuffer();

        /**
         * @brief Clears all internal buffers.
         */
        void clearAllBuffers();

        /**
         * @brief Checks if history buffer is empty.
         * @return True if no history.
         */
        bool isEmptyHistory() const;
        friend class LotusEngine;

        /**
         * @brief Commits text still waiting for the app before the input context loses focus.
         * Without it, a timer firing later sees is_deleting_ cleared and silently drops the text.
         */
        void flushPendingReplacement();

      private:
        static constexpr size_t  MAX_BUFFERED_KEYS = 50;

        ngosen::EngineResources* engine_;
        CGoObject                lotusEngine_;
        std::string              oldPreBuffer_;
        bool                     hasHistory_              = false;
        int                      expected_backspaces_     = 0;
        int                      current_backspace_count_ = 0;
        std::string              pending_commit_string_;
        std::string              emojiBuffer_;
        std::vector<EmojiEntry>  emojiCandidates_;
        std::vector<KeyEntry>    buffered_keys_; ///< Keystrokes buffered during replacement
        bool                     isPrevSpace_           = false;
        bool                     isPrevHyphen_          = false;
        bool                     shouldCapitalize_      = false;
        bool                     isPrevPunctuation_     = false;
        int64_t                  lastDeactivateTime_    = 0;
        int64_t                  deletionInterruptedAt_ = 0;     ///< when deactivate() cut an in-flight replacement (0 = none)
        bool                     tracking_modifier_tap_ = false; ///< Selected modifier held, waiting for consecutive keyup
        bool                     macro_skip_            = false; ///< Macro disabled for the current word

        // Presses real keys through XTEST, for frontends that cannot forward them.
        void sendBackspaceKeys(int count) const;

        // A report while waiting for the app to apply backspaces or show a selection.
        void onWaitSurroundingUpdated();
        void onOvertypeSurroundingUpdated();

        // --- Sen mode: wait for the app instead of sleeping (see handleUInputKeyPress) ---
        std::unique_ptr<ngosen::Timer> surr_wait_timer_;
        bool                           surr_wait_watching_ = false; ///< reports go to onWaitSurroundingUpdated once a wait has started
        std::unique_ptr<ngosen::Timer> xim_forward_timer_;          ///< forwards after the XIM sync reply
        uint64_t                       surr_wait_started_at_    = 0;
        uint64_t                       surr_wait_deliver_at_    = 0; ///< planned commit time, CLOCK_MONOTONIC us
        bool                           surr_wait_pending_       = false;
        bool                           surr_wait_timer_only_    = false; ///< waiting on a plain timer that replaces sleep_for
        int                            surr_wait_focus_retries_ = 0;     ///< timer fired while the field had lost focus
        std::string                    surr_wait_prefix_;                ///< part of the word kept after deletion
        std::string                    surr_wait_deleted_;               ///< part that must disappear
        std::string                    surr_wait_sent_snapshot_;         ///< "text\x1fcursor" when the backspaces were sent
        int                            surr_wait_event_count_         = 0;
        bool                           surr_wait_saw_other_snapshot_  = false; ///< an event differed from the send-time snapshot
        bool                           surr_wait_sent_snapshot_fresh_ = false; ///< send-time snapshot still showed the text to delete

        // "Frozen": a wait timed out and every event matched the send-time snapshot. After two in a
        // row, sleep instead and probe again every WaitSurroundingProbeEvery replacements.
        int  surr_frozen_streak_      = 0;
        bool surr_frozen_             = false;
        int  surr_frozen_probe_count_ = 0;
        int  surr_timeout_streak_     = 0;    ///< two or more switches to the short timeout
        bool surr_snapshot_trusted_   = true; ///< false after a timeout, true again on a matching event
        bool deletionLooksDone() const;

        // Messenger repaints its composer a few ms after the snapshot shows the deletion done, and
        // text committed before that repaint is overwritten. Delay the commit by
        // WaitSurroundingSettleMs (0 = commit immediately).
        void                           deliverAfterSettle(const char* reason, bool fromTimer);
        std::unique_ptr<ngosen::Timer> settle_timer_;
        const char*                    settle_reason_ = "";

        void                           finishReplacement(const char* reason, bool fromTimer);
        // fcitx5-gtk sends text committed while it processes a key event in the reply to that key.
        // The sentinel Backspace is consumed, and some clients (ghostty, foot) then drop the text.
        // Commit on the next event loop turn instead. Upstream fcitx5-lotus 2ca89a5.
        void                           flushDeferredCommit();
        std::unique_ptr<ngosen::Timer> deferred_commit_timer_;
        std::string                    deferred_commit_text_;
        bool                           deferred_commit_pending_ = false;

        // Last surrounding text the app reported, to tell a click from an edit.
        std::string  lastSurroundingText_;
        unsigned int lastSurroundingCursor_  = 0;
        unsigned int lastSurroundingAnchor_  = 0;
        bool         hasLastSurrounding_     = false;
        unsigned int unreportedCommitLength_ = 0;
        uint64_t     lastInputAtUs_          = 0; ///< last key or commit, CLOCK_MONOTONIC us

        // Last key press seen, to spot one the app sends twice.
        uint32_t lastPressCode_ = 0;
        uint32_t lastPressTime_ = 0;

        // --- Select and overtype (Facebook composers) ---
        // Select with Shift+Left, wait for the snapshot to show the selection, then commit over it.
        // The field never becomes empty, so Messenger does not reload its placeholder.
        void                           sendSelectKeys(int charCount) const; // sent as a negative count
        void                           selectAndOvertype(const std::string& addedPart, int charCount, bool viaXTest = false);
        void                           finishOvertype(const char* reason, bool fromTimer);
        void                           abandonOvertype();
        std::unique_ptr<ngosen::Timer> overtype_timer_;
        bool                           overtype_watching_       = false; ///< reports go to onOvertypeSurroundingUpdated once an overtype has started
        bool                           overtype_pending_        = false;
        unsigned int                   overtype_cursor_before_  = 0;
        bool                           overtype_had_snapshot_   = false;
        int                            overtype_char_count_     = 0;
        uint64_t                       overtype_started_at_     = 0;
        bool                           overtype_via_xtest_      = false;
        bool                           overtype_shift_released_ = false;

        /**
         * @brief Checks if autofill is certain for surrounding text.
         * @param s The surrounding text.
         * @return True if autofill should proceed.
         */
        bool isAutofillCertain(const ngosen::Surrounding& s);

        /**
         * @brief Handles key events in preedit mode.
         * @param keyEvent The key event to process.
         * @param currentSym Current key symbol.
         */
        void handlePreeditMode(ngosen::KeyPress& keyEvent, uint32_t currentSym);

        /**
         * @brief Updates emoji page status in candidate list.
         */
        void updateEmojiPageStatus();
        // Commits a picked emoji and records it in the history.
        void pickEmoji(const EmojiEntry& entry);

        /**
         * @brief Handles key events in emoji mode.
         * @param keyEvent The key event to process.
         */
        void handleEmojiMode(ngosen::KeyPress& keyEvent);

        /**
         * @brief Updates preedit display for emoji mode.
         */
        void updateEmojiPreedit();

        /**
         * @brief Handles key press in Sen mode.
         * @param event The key event.
         * @param currentSym Current key symbol.
         * @param sleepTime Delay in microseconds.
         * @return True if event was handled.
         */
        bool handleUInputKeyPress(ngosen::KeyPress& event, uint32_t currentSym, int sleepTime);
        bool waitForDeletion(ngosen::KeyPress* event, int sleepTime);
        void forwardBackspaces(int count);
        // True when a replacement can delete text: forwarded by the frontend or pressed through XTEST.
        bool canSendBackspaces() const;
        // Moves the commit of a timer-only wait later, never earlier.
        void deferTimedCommit(uint64_t deliverAtUs);

        /**
         * @brief Replaces text by sending backspaces, then committing.
         * @param deletedPart Text to delete.
         * @param addedPart Text to insert.
         */
        void performReplacement(const std::string& deletedPart, const std::string& addedPart);

        /**
         * @brief Handles the double space to period replacement.
         */
        void handleDoubleSpaceReplacement();

        /**
         * @brief Handles the double hyphen to em-dash replacement.
         */
        void handleDoubleHyphenReplacement();

        /**
         * @brief Checks and forwards special keys.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol (may be modified).
         * @return True if key was forwarded.
         */
        bool checkForwardSpecialKey(ngosen::KeyPress& keyEvent, uint32_t& currentSym);

        /**
         * @brief Handles Sen mode processing.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         * @param sleepTime Delay in microseconds.
         */
        void handleUinputMode(ngosen::KeyPress& keyEvent, uint32_t currentSym);

        /**
         * @brief Handles Off mode with macro shadow processing.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         */
        void handleOffModeMacro(ngosen::KeyPress& keyEvent, uint32_t currentSym);

        /**
         * @brief Replays keystrokes buffered during replacement.
         *
         * When is_deleting_ is true, non-special keystrokes are buffered
         * instead of being discarded. This method replays them after the
         * replacement completes.
         * @param committed Text to commit first; it goes out in the same commit as the replayed keys.
         */
        void replayBufferedKeys(std::string committed = {});

        /**
         * @brief Checks if the key symbol matches the configured macro-skip modifier.
         * @param sym Key symbol to check.
         * @return True if the key is the configured trigger modifier (left/right same).
         */
        bool isMacroSkipModifier(uint32_t sym) const;

        /**
         * @brief Tracks a modifier tap (keydown then consecutive keyup) to skip macro.
         * @param keyEvent The modifier key event.
         */
        void handleModifierTap(const ngosen::KeyPress& keyEvent);

        /**
         * @brief Cancels an in-progress modifier tap when another key arrives.
         */
        void cancelModifierTap();

        /**
         * @brief Re-enables macro when the current word ends (engine preedit empty).
         */
        void reEnableMacroAfterWordEnd();

        /**
         * @brief Clears the macro-skip state and re-syncs the engine.
         */
        void resetMacroSkip();

        // Everything sent to the app goes through here, so the typing logic does not call fcitx5 directly.
        std::unique_ptr<ngosen::Host> host_;
    };

} // namespace fcitx

#endif // _FCITX5_LOTUS_STATE_H_
