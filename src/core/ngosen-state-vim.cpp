/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Vim's normal mode in a terminal: the app reports no text, so commands such as "ciw" would become
// the start of the next word and keep it from getting tone marks (#302).
#include "ngosen-state.h"

#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ngosen {

    void TypingState::setVimKeys(bool on) {
        vimKeys_ = on;
        if (!vimOn())
            vimState_ = VimState::Insert;
    }

    void TypingState::enterVimNormalMode() {
        vimState_ = VimState::Normal;
        vimReader_.reset();
        vimTyped_.clear();
        vimLine_.clear();
    }

    // Vim exits and the shell takes text again.
    void TypingState::leaveVim() {
        vimAuto_  = false;
        vimState_ = VimState::Insert;
        vimReader_.reset();
        vimTyped_.clear();
        vimLine_.clear();
    }

    void TypingState::noteShellKey(const ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (vimAuto_ || vimState_ != VimState::Insert || keyEvent.isModifier() || host_->field().surroundingText)
            return;
        if (currentSym == ngosen::key::Return || currentSym == ngosen::key::KP_Enter) {
            const bool starts = startsVim(vimLine_);
            vimLine_.clear();
            if (starts) {
                vimAuto_ = true;
                enterVimNormalMode();
                NGOSEN_DEBUG("Vim starts from the shell");
            }
            return;
        }
        if (ngosen::key::isBackspace(currentSym) && !keyEvent.hasModifier()) {
            ngosen::utf8::eraseLastCodepoint(vimLine_);
            return;
        }
        // Tab completion, history and cursor keys change the line in ways we cannot follow.
        if (typesText(keyEvent, currentSym))
            vimLine_ += host_->keyText(currentSym);
        else
            vimLine_.clear();
    }

    void TypingState::startWordAfterVimCommand() {
        vimTyped_.clear();
        vimLine_.clear();
        hasHistory_ = false;
        lastWord_.clear();
        ResetEngine(bambooEngine_.handle());
        oldPreBuffer_.clear();
    }

    // Not a Vim command after all: the letters let through since the last break are the start of a
    // word. The engine takes them as typed keys, and the app shows them raw.
    void TypingState::takeUpVimTyped() {
        const auto        breakAt = std::find_if(vimTyped_.rbegin(), vimTyped_.rend(), [](char c) { return std::isalpha(static_cast<unsigned char>(c)) == 0; });
        const std::string word(breakAt.base(), vimTyped_.end());
        vimTyped_.clear();
        for (const char c : word)
            EngineProcessKeyEvent(bambooEngine_.handle(), static_cast<unsigned char>(c), 0);
        oldPreBuffer_ = word;
        hasHistory_   = !word.empty();
    }

    bool TypingState::readVimKey(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (vimState_ == VimState::CommandLine) {
            // The line takes text until Enter runs it; Backspace on the bare ":" closes it.
            if (currentSym == ngosen::key::Return || currentSym == ngosen::key::KP_Enter) {
                if (quitsVim(vimLine_))
                    leaveVim();
                else
                    enterVimNormalMode();
            } else if (ngosen::key::isBackspace(currentSym)) {
                if (vimLine_.size() <= 1)
                    enterVimNormalMode();
                else
                    ngosen::utf8::eraseLastCodepoint(vimLine_);
            } else if (typesText(keyEvent, currentSym)) {
                vimLine_ += host_->keyText(currentSym);
            }
            return false;
        }
        // These move the cursor in normal mode; Backspace must not take up the last word again.
        if (ngosen::key::isBackspace(currentSym) || currentSym == ngosen::key::Return) {
            lastWord_.clear();
            keyEvent.passToApp();
            return true;
        }
        const std::string text = host_->keyText(currentSym);
        if (text.size() != 1 || !typesText(keyEvent, currentSym))
            return false;
        switch (vimReader_.feed(text[0])) {
            case ngosen::VimStep::Unknown:
                vimState_ = VimState::Insert;
                // Preedit already let those letters through as they are; only Sen rewrites the word in place.
                if (realMode == ngosen::Mode::Sen)
                    takeUpVimTyped();
                else
                    vimTyped_.clear();
                return false;
            case ngosen::VimStep::Pending: vimTyped_ += text; break;
            case ngosen::VimStep::Quit:
                leaveVim();
                startWordAfterVimCommand();
                break;
            case ngosen::VimStep::CommandLine:
                vimState_ = VimState::CommandLine;
                startWordAfterVimCommand();
                vimLine_ = text;
                break;
            case ngosen::VimStep::Insert:
                vimState_ = VimState::Insert;
                startWordAfterVimCommand();
                NGOSEN_DEBUG(std::string("Vim goes into insert mode at ") + text);
                break;
        }
        keyEvent.passToApp();
        return true;
    }

} // namespace ngosen
