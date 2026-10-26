/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: a word that starts with '/' goes to the app as typed.
#include "ngosen-keysym.h"
#include "ngosen-state.h"
#include "ngosen-utf8.h"

#include <string>

namespace ngosen {

    namespace {
        bool continuesRawWord(uint32_t sym) {
            if ((sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z') || (sym >= '0' && sym <= '9'))
                return true;
            switch (sym) {
                case '/':
                case '.':
                case '_':
                case '-':
                case '~':
                case ':': return true;
                default: return false;
            }
        }

        bool endsWord(uint32_t sym) {
            return sym == key::space || sym == key::KP_Space || sym == key::Return || sym == key::KP_Enter || sym == key::Tab || sym == key::KP_Tab;
        }
    } // namespace

    bool TypingState::atRawWordStart() const {
        // A '/' inside a word, as in và/hoặc or km/h, is still Vietnamese.
        const UniqueCPtr<char> word(EnginePullPreedit(bambooEngine_.handle()));
        if (word && *word.get() != 0)
            return false;
        const Field       field = host_->field();
        const Surrounding s     = host_->surrounding();
        if (!field.surroundingText || keySinceReport_ || !s.isValid() || s.cursor() != s.anchor())
            return afterWordEnd_;
        if (s.cursor() == 0)
            return true;
        const std::u32string text = utf8::decode(s.text());
        if (s.cursor() > text.size())
            return afterWordEnd_;
        const char32_t before = text[s.cursor() - 1];
        return before == U' ' || before == U'\t' || before == U'\n';
    }

    // Commands and paths filter as they are typed, so each key must reach the app unchanged.
    bool TypingState::handleRawWordKey(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (keyEvent.isRelease() || keyEvent.isModifier())
            return false;
        if (!engine_->options().rawWordAfterSlash || keyEvent.hasModifier()) {
            rawWord_ = false;
            return false;
        }
        if (rawWord_) {
            if (key::isBackspace(currentSym)) {
                if (rawWordLen_ == 0)
                    rawWord_ = false;
                else
                    --rawWordLen_;
                keyEvent.passToApp();
                return true;
            }
            if (continuesRawWord(currentSym)) {
                ++rawWordLen_;
                keyEvent.passToApp();
                return true;
            }
            rawWord_ = false;
        } else if (currentSym == '/' && atRawWordStart()) {
            rawWord_    = true;
            rawWordLen_ = 0;
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
            keyEvent.passToApp();
            return true;
        }
        afterWordEnd_ = endsWord(currentSym);
        return false;
    }

} // namespace ngosen
