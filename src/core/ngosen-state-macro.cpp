/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
// TypingState: macros, double space to period, double hyphen to em dash, and the key that locks a word.
#include "ngosen-state.h"
#include "ngosen-clock.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <cstddef>
#include <string>

namespace ngosen {

    void TypingState::handleDoubleSpaceReplacement() {
        performReplacement(oldPreBuffer_, " ", ". ");
        NGOSEN_INFO("Commit: . ");
        if (engine_->options().autoCapitalizeAfterPunctuation) {
            isPrevPunctuation_ = true;
            shouldCapitalize_  = true;
        }
    }

    void TypingState::handleDoubleHyphenReplacement() {
        // Em-dash (U+2014)
        std::string emDash = "—";
        performReplacement(oldPreBuffer_, "-", emDash);
        NGOSEN_INFO("Commit: — (em-dash)");
    }

    bool TypingState::finishWordBeforeEnter(ngosen::KeyPress& keyEvent) {
        if (oldPreBuffer_.empty())
            return false;
        EngineProcessKeyEvent(bambooEngine_.handle(), ngosen::key::Return, keyEvent.states());
        const auto pulled = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()));
        if (!pulled || *pulled.get() == 0)
            return false;
        const std::string expansion = spelledAtWordEnd(pulled.get());
        if (oldPreBuffer_ == expansion)
            return false;
        std::string deletedPart;
        std::string addedPart;
        ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, expansion, deletedPart, addedPart);
        // Same limits as a word a space ends: a field that cannot take the edit keeps the typed word.
        if (!deletedPart.empty() && (!canSendBackspaces() || keepsWordRaw(deletedPart))) {
            rawWord_ = false;
            lastWord_.clear();
            return false;
        }
        NGOSEN_INFO("Word before Enter: " + logged(oldPreBuffer_) + " -> " + logged(expansion));
        hasHistory_ = false;
        ResetEngine(bambooEngine_.handle());
        oldPreBuffer_.clear();
        lastWord_.clear();
        if (deletedPart.empty()) {
            commit(addedPart);
            keyEvent.passToApp();
            return true;
        }
        enter_after_replacement_ = true;
        enter_after_key_         = ngosen::EditKey::Return;
        enter_key_live_          = true;
        performReplacement(oldPreBuffer_, deletedPart, addedPart);
        enter_key_live_ = false;
        // Done already, so the Enter can go to the app as typed.
        if (!enter_after_replacement_)
            keyEvent.passToApp();
        else
            keyEvent.accept();
        return true;
    }

    void TypingState::sendEnterAfterReplacement() {
        enter_after_replacement_ = false;
        if (!enter_key_live_)
            forwardTap(enter_after_key_);
    }

    void TypingState::handleOffModeMacro(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.passToApp();
            return;
        }

        if (ngosen::key::isBackspace(currentSym)) {
            EngineProcessKeyEvent(bambooEngine_.handle(), ngosen::key::BackSpace, 0);
            auto preeditC = ngosen::UniqueCPtr<char>(EnginePullPreedit(bambooEngine_.handle()));
            oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            keyEvent.passToApp();
            return;
        }

        if (currentSym == ngosen::key::Return) {
            if (!oldPreBuffer_.empty()) {
                ResetEngine(bambooEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.passToApp();
            return;
        }

        std::string keyUtf8 = host_->keyText(currentSym);

        bool        processed = EngineProcessKeyEvent(bambooEngine_.handle(), currentSym, keyEvent.states()) != 0U;

        auto        commitPtr = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()));
        if (processed && commitPtr && (*commitPtr.get() != 0)) {
            expandOffModeMacro(keyEvent, keyUtf8, commitPtr.get());
            oldPreBuffer_.clear();
            hasHistory_ = false;
            return;
        }

        // 8. No commit or engine rejected the key
        if (processed || (commitPtr && (*commitPtr.get() != 0))) {
            // Engine processed the key (building shadow state)
            // OR engine rejected the key but committed old text (non-processable key)
            auto preeditPtr = ngosen::UniqueCPtr<char>(EnginePullPreedit(bambooEngine_.handle()));
            oldPreBuffer_   = (preeditPtr && (*preeditPtr.get() != 0)) ? preeditPtr.get() : "";
            if (!processed) {
                // Engine committed old text but didn't process the new key → forward the key
                oldPreBuffer_.clear();
                hasHistory_ = false;
            }
            keyEvent.passToApp();
        } else {
            // Engine didn't handle this key
            if (!oldPreBuffer_.empty()) {
                ResetEngine(bambooEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.passToApp();
        }
    }

    void TypingState::expandOffModeMacro(ngosen::KeyPress& keyEvent, const std::string& keyUtf8, const std::string& commitStr) {
        // Determine if this is a macro expansion or just confirmed typed text
        bool isMacroExpansion = false;
        if (keyUtf8.empty()) {
            isMacroExpansion = (commitStr != oldPreBuffer_);
        } else {
            isMacroExpansion = (commitStr != oldPreBuffer_ + keyUtf8);
        }

        if (isMacroExpansion) {
            NGOSEN_INFO("Macro expansion: '" + logged(oldPreBuffer_) + "' -> '" + logged(commitStr) + "'");
            // The field now holds the expansion, not the keys noted for a retype.
            forgetKeysWhileOff();
            if (!canReplaceInApp()) {
                keyEvent.passToApp();
                return;
            }
            if (canSendBackspaces() && !oldPreBuffer_.empty()) {
                performReplacement(oldPreBuffer_, oldPreBuffer_, commitStr);
            } else if (host_->surrounding().isValid()) {
                const size_t oldLen = ngosen::utf8::length(oldPreBuffer_);
                if (oldLen > 0)
                    deleteBeforeCursor(oldLen);
                commit(commitStr);
            } else {
                // The abbreviation stays in the field, so the expansion would land after it.
                keyEvent.passToApp();
                return;
            }
            keyEvent.accept();
        } else {
            // No macro: typed text confirmed by engine, just forward trigger key
            keyEvent.passToApp();
        }
    }

    bool TypingState::isLockWordModifier(uint32_t sym) const {
        switch (engine_->options().lockWordKey) {
            case ngosen::LockWordKey::Shift: return sym == ngosen::key::Shift_L || sym == ngosen::key::Shift_R;
            case ngosen::LockWordKey::Ctrl: return sym == ngosen::key::Control_L || sym == ngosen::key::Control_R;
            case ngosen::LockWordKey::Alt: return sym == ngosen::key::Alt_L || sym == ngosen::key::Alt_R;
            case ngosen::LockWordKey::None:
            default: return false;
        }
    }

    void TypingState::handleModifierTap(const ngosen::KeyPress& keyEvent) {
        if (!isLockWordModifier(keyEvent.sym())) {
            tracking_modifier_tap_ = false;
            // A waiting swap would go out with this modifier held.
            if (!keyEvent.isRelease())
                lockSwapDue_ = false;
            return;
        }
        if (!keyEvent.isRelease()) {
            // Releasing Ctrl first from a held Shift+Ctrl is a chord, not a tap.
            constexpr uint32_t held = ngosen::modifier::Shift | ngosen::modifier::Ctrl | ngosen::modifier::Alt | ngosen::modifier::Super;
            tracking_modifier_tap_  = (keyEvent.states() & held) == 0;
            // Pressed again before the swap ran: the swap would go out with Ctrl down, and this tap takes it back.
            tapTakesBackSwap_ = std::exchange(lockSwapDue_, false);
            return;
        }
        if (!tracking_modifier_tap_)
            return;
        tracking_modifier_tap_ = false;
        if (std::exchange(tapTakesBackSwap_, false)) {
            NGOSEN_INFO("Locked word swap taken back");
            return;
        }
        // Keys typed while a replacement is in flight replay after it; lock the word once they have.
        if (!is_deleting_.load(std::memory_order_acquire))
            lockWord();
        else if (lockedWord_)
            // Swapping again only once the app shows this swap keeps the deletion off text not there yet.
            NGOSEN_INFO("Tap during a swap dropped");
        else
            lock_word_pending_ = true;
    }

    void TypingState::cancelModifierTap() {
        tracking_modifier_tap_ = false;
    }

    void TypingState::lockWord() {
        lock_word_pending_ = false;
        if (lockedWord_ && lockedWordStillThere())
            swapLockedWordSoon();
        if (lockedWord_)
            return;
        // A tap between words leaves the last word open to Backspace.
        const uintptr_t          engine = bambooEngine_.handle();
        ngosen::UniqueCPtr<char> preedit(EnginePullPreedit(engine));
        if (!preedit || *preedit.get() == 0)
            return;
        ngosen::UniqueCPtr<char> keys(EnginePullRawText(engine));
        ngosen::UniqueCPtr<char> composed(EnginePullComposed(engine));
        LockedWord               word{.keys = keys ? keys.get() : "", .composed = composed ? composed.get() : "", .showsComposed = true};
        // A word shown as typed is locked with the marks its keys make, since keeping the keys needs no lock.
        word.showsComposed = word.composed.empty() || word.composed == word.keys || word.keys != preedit.get();
        if (!word.showsComposed && mode() == ngosen::Mode::Preedit)
            commitComposedPreedit(word.composed);
        else
            commitBuffer();
        // The word is already in the app; the engine only has to forget it. Off mode keeps a word for macros too.
        ResetEngine(engine);
        oldPreBuffer_.clear();
        lastWord_.clear();
        hasHistory_ = false;
        NGOSEN_INFO("Word locked");
        // A preedit field commits on the tap, so the word is no longer there to swap.
        if (mode() != ngosen::Mode::Sen)
            return;
        word.editedAtUs = ngosen::monotonicUs();
        lockedWord_     = word;
        if (!word.showsComposed)
            swapLockedWordSoon();
    }

    // Backspaces sent before the app sees the lock key go up would delete as Ctrl+BackSpace, the whole word.
    void TypingState::swapLockedWordSoon() {
        constexpr uint64_t LockKeyUpUs = 30000;
        lockSwapDue_                   = true;
        lock_swap_timer_               = host_->startTimer(ngosen::monotonicUs() + LockKeyUpUs, 0, [this](ngosen::Timer&) {
            runDueLockSwap();
            return false;
        });
    }

    void TypingState::runDueLockSwap() {
        if (!std::exchange(lockSwapDue_, false))
            return;
        if (lockedWord_ && !is_deleting_.load(std::memory_order_acquire) && lockedWordStillThere())
            swapLockedWord();
    }

    void TypingState::commitComposedPreedit(const std::string& composed) {
        host_->resetPanel();
        commit(composed);
        ResetEngine(bambooEngine_.handle());
        host_->refreshPanel();
        host_->refreshPreedit();
    }

    // A click or a field that shows other text before the cursor means the word is no longer where it was.
    bool TypingState::lockedWordStillThere() {
        if (g_mouse_clicked.load(std::memory_order_acquire)) {
            lockedWord_.reset();
            return false;
        }
        const Surrounding s = host_->surrounding();
        // Konsole reports a valid empty text, which says nothing about the word.
        if (!host_->field().surroundingText || !s.isValid() || s.text().empty())
            return true;
        // A late report may still show the form before the last swap.
        const std::string before = textBeforeCursor(s);
        if (isEndsWith(before, lockedWord_->keys) || isEndsWith(before, lockedWord_->composed))
            return true;
        lockedWord_.reset();
        return false;
    }

    // Each tap after the lock swaps the word between the keys typed and the marks they make.
    void TypingState::swapLockedWord() {
        LockedWord& word = *lockedWord_;
        if (word.composed.empty() || word.composed == word.keys)
            return;
        std::string deletedPart;
        std::string addedPart;
        ngosen::utf8::compareAndSplitStrings(word.showsComposed ? word.composed : word.keys, word.showsComposed ? word.keys : word.composed, deletedPart, addedPart);
        word.showsComposed = !word.showsComposed;
        word.editedAtUs    = ngosen::monotonicUs();
        NGOSEN_INFO(std::string("Locked word swapped to its ") + (word.showsComposed ? "marks" : "keys"));
        performReplacement(oldPreBuffer_, deletedPart, addedPart);
    }

} // namespace ngosen
