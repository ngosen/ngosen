/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Vim's normal mode in a terminal: the app reports no text, so commands such as "ciw" would become
// the start of the next word and keep it from getting tone marks (#302).
#include "ngosen-state.h"

#include "ngosen-keysym.h"
#include "ngosen-log.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ngosen {

    void TypingState::setVimKeys(bool on) {
        vimKeys_ = on;
        if (!on)
            vimState_ = VimState::Insert;
    }

    void TypingState::enterVimNormalMode() {
        vimState_ = VimState::Normal;
        vimReader_.reset();
        vimTyped_.clear();
    }

    void TypingState::startWordAfterVimCommand() {
        vimTyped_.clear();
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
            // Enter runs the command line and Vim goes back to normal mode; until then the line takes text.
            if (currentSym == ngosen::key::Return)
                enterVimNormalMode();
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
        const ngosen::VimStep step = vimReader_.feed(text[0]);
        if (step == ngosen::VimStep::Unknown) {
            vimState_ = VimState::Insert;
            takeUpVimTyped();
            return false;
        }
        if (step == ngosen::VimStep::Pending) {
            vimTyped_ += text;
        } else {
            vimState_ = step == ngosen::VimStep::CommandLine ? VimState::CommandLine : VimState::Insert;
            startWordAfterVimCommand();
            NGOSEN_DEBUG(std::string("Vim leaves normal mode at ") + text);
        }
        keyEvent.passToApp();
        return true;
    }

} // namespace ngosen
