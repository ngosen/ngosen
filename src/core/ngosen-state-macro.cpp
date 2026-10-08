/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
// TypingState: macros, double space to period, double hyphen to em dash, and the macro skip key.
#include "ngosen-state.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-utf8.h"

#include <cstddef>
#include <string>

namespace ngosen {

    void TypingState::handleDoubleSpaceReplacement() {
        performReplacement(" ", ". ");
        NGOSEN_INFO("Commit: . ");
        if (engine_->options().autoCapitalizeAfterPunctuation) {
            isPrevPunctuation_ = true;
            shouldCapitalize_  = true;
        }
    }

    void TypingState::handleDoubleHyphenReplacement() {
        // Em-dash (U+2014)
        std::string emDash = "—";
        performReplacement("-", emDash);
        NGOSEN_INFO("Commit: — (em-dash)");
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
            std::string commitStr = commitPtr.get();

            // Determine if this is a macro expansion or just confirmed typed text
            bool isMacroExpansion = false;
            if (keyUtf8.empty()) {
                isMacroExpansion = (commitStr != oldPreBuffer_);
            } else {
                isMacroExpansion = (commitStr != oldPreBuffer_ + keyUtf8);
            }

            if (isMacroExpansion) {
                NGOSEN_INFO("Macro expansion: '" + oldPreBuffer_ + "' -> '" + commitStr + "'");
                // Try backspaces first, fallback to deleteSurroundingText, then plain commit
                if (canSendBackspaces() && !oldPreBuffer_.empty()) {
                    performReplacement(oldPreBuffer_, commitStr);
                } else if (host_->field().surroundingText) {
                    const auto surrounding = host_->surrounding();
                    if (surrounding.isValid()) {
                        size_t oldLen = ngosen::utf8::length(oldPreBuffer_);
                        if (oldLen > 0) {
                            host_->deleteSurrounding(-static_cast<int>(oldLen), static_cast<int>(oldLen));
                        }
                        host_->commitText(commitStr);
                    } else {
                        host_->commitText(commitStr);
                    }
                } else {
                    host_->commitText(commitStr);
                }
                keyEvent.accept();
            } else {
                // No macro: typed text confirmed by engine, just forward trigger key
                keyEvent.passToApp();
            }

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

    bool TypingState::isMacroSkipModifier(uint32_t sym) const {
        const auto trigger = engine_->options().macroSkipKey;
        switch (trigger) {
            case ngosen::MacroSkipKey::Shift: return sym == ngosen::key::Shift_L || sym == ngosen::key::Shift_R;
            case ngosen::MacroSkipKey::Ctrl: return sym == ngosen::key::Control_L || sym == ngosen::key::Control_R;
            case ngosen::MacroSkipKey::Alt: return sym == ngosen::key::Alt_L || sym == ngosen::key::Alt_R;
            case ngosen::MacroSkipKey::None:
            default: return false;
        }
    }

    void TypingState::handleModifierTap(const ngosen::KeyPress& keyEvent) {
        const auto trigger = engine_->options().macroSkipKey;
        if (trigger == ngosen::MacroSkipKey::None || !engine_->options().enableMacro) {
            return;
        }
        if (!isMacroSkipModifier(keyEvent.sym())) {
            tracking_modifier_tap_ = false;
            return;
        }
        if (keyEvent.isRelease()) {
            if (tracking_modifier_tap_) {
                tracking_modifier_tap_ = false;
                macro_skip_            = true;
                EngineSetMacroEnabled(bambooEngine_.handle(), 0);
                NGOSEN_INFO("Macro skip enabled for next word");
            }
        } else {
            tracking_modifier_tap_ = true;
        }
    }

    void TypingState::cancelModifierTap() {
        tracking_modifier_tap_ = false;
    }

    void TypingState::reEnableMacroAfterWordEnd() {
        if (!macro_skip_) {
            return;
        }
        ngosen::UniqueCPtr<char> preedit(EnginePullPreedit(bambooEngine_.handle()));
        if (preedit && *preedit.get() != 0) {
            return;
        }
        macro_skip_ = false;
        EngineSetMacroEnabled(bambooEngine_.handle(), engine_->options().enableMacro ? 1 : 0);
    }

    void TypingState::resetMacroSkip() {
        tracking_modifier_tap_ = false;
        macro_skip_            = false;
        if (bambooEngine_) {
            EngineSetMacroEnabled(bambooEngine_.handle(), engine_->options().enableMacro ? 1 : 0);
        }
    }

} // namespace ngosen
