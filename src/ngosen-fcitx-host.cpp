/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-fcitx-host.h"

#include "ngosen-xtest.h"

#include <fcitx-utils/key.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>

namespace ngosen {

    namespace {
        // XKB keycode of BackSpace: evdev KEY_BACKSPACE (14) + 8.
        constexpr int BackSpaceKeycode = 22;

        fcitx::Key    toFcitxKey(EditKey key) {
            switch (key) {
                case EditKey::BackSpace: return fcitx::Key(FcitxKey_BackSpace, fcitx::KeyStates(), BackSpaceKeycode);
                case EditKey::Right: return fcitx::Key(FcitxKey_Right);
            }
            return fcitx::Key();
        }
    } // namespace

    void FcitxHost::commitText(const std::string& text) {
        ic_->commitString(text);
    }

    void FcitxHost::forwardKey(EditKey key, bool release) {
        ic_->forwardKey(toFcitxKey(key), release);
    }

    void FcitxHost::deleteSurrounding(int offset, unsigned int size) {
        ic_->deleteSurroundingText(offset, size);
    }

    bool FcitxHost::pressSystemKeys(int count) {
        return xtestSendKeys(count);
    }

    Surrounding FcitxHost::surrounding() const {
        const auto& s = ic_->surroundingText();
        if (!s.isValid()) {
            return {};
        }
        return {s.text(), s.cursor(), s.anchor()};
    }

    Field FcitxHost::field() const {
        const auto& caps = ic_->capabilityFlags();
        Field       f;
        f.frontend         = ic_->frontend();
        f.program          = ic_->program();
        f.surroundingText  = caps.test(fcitx::CapabilityFlag::SurroundingText);
        f.preedit          = caps.test(fcitx::CapabilityFlag::Preedit);
        f.formattedPreedit = caps.test(fcitx::CapabilityFlag::FormattedPreedit);
        f.url              = caps.test(fcitx::CapabilityFlag::Url);
        f.keyEventOrderFix = caps.test(fcitx::CapabilityFlag::KeyEventOrderFix);
        return f;
    }

    bool FcitxHost::hasFocus() const {
        return ic_->hasFocus();
    }

    void FcitxHost::showPreedit(const std::string& text, bool underline) {
        fcitx::Text preedit;
        if (!text.empty())
            preedit.append(text, underline ? fcitx::TextFormatFlag::Underline : fcitx::TextFormatFlag::NoFlag);
        preedit.setCursor(static_cast<int>(preedit.textLength()));
        if (ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit))
            ic_->inputPanel().setClientPreedit(preedit);
        else
            ic_->inputPanel().setPreedit(preedit);
    }

    void FcitxHost::clearPreedit() {
        ic_->inputPanel().setClientPreedit(fcitx::Text());
        ic_->inputPanel().setPreedit(fcitx::Text());
    }

    void FcitxHost::resetPanel() {
        ic_->inputPanel().reset();
    }

    void FcitxHost::refreshPreedit() {
        ic_->updatePreedit();
    }

    void FcitxHost::refreshPanel() {
        ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

} // namespace ngosen
