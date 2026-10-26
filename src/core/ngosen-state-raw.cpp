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
            return sym > ' ' && sym < 0x7f;
        }

        bool endsWord(uint32_t sym) {
            return sym == key::space || sym == key::KP_Space || sym == key::Return || sym == key::KP_Enter || sym == key::Tab || sym == key::KP_Tab;
        }

        // May come before the '/' of a path or command: ./x, ../x, ~/x, `/cost`, (/x).
        bool leadsRawWord(char32_t c) {
            return c == '.' || c == '~' || c == '`' || c == '(' || c == '"' || c == '\'';
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
        const std::string& t  = s.text();
        auto               it = t.begin();
        bool               at = true;
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
            const auto next = utf8::nextChar(it, t.end());
            const char c    = next - it == 1 ? *it : '\0';
            at              = c == ' ' || c == '\t' || c == '\n' || (at && leadsRawWord(static_cast<char32_t>(c)));
            it              = next;
        }
        return at;
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
            } else if (continuesRawWord(currentSym)) {
                ++rawWordLen_;
            } else {
                rawWord_      = false;
                afterWordEnd_ = endsWord(currentSym);
                return false;
            }
        } else if (currentSym == '/' && atRawWordStart()) {
            rawWord_    = true;
            rawWordLen_ = 0;
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
        } else {
            afterWordEnd_ = endsWord(currentSym) || (afterWordEnd_ && leadsRawWord(currentSym));
            return false;
        }
        isPrevSpace_  = false;
        isPrevHyphen_ = false;
        // Firefox applies a key it let through ahead of a commit it already has.
        if (realMode == ngosen::Mode::Sen && typedBeforeCommitArrived(keyEvent, currentSym)) {
            keyEvent.accept();
            commit(host_->keyText(currentSym));
        } else {
            keyEvent.passToApp();
        }
        return true;
    }

} // namespace ngosen
