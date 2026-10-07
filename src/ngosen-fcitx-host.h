/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"
#include "ngosen-key.h"

#include <memory>

namespace fcitx {
    class CommonCandidateList;
    class InputContext;
    class Instance;
    class KeyEvent;
}

namespace ngosen {

    class FcitxHost final : public Host {
      public:
        FcitxHost(fcitx::InputContext* ic, fcitx::Instance* instance) : ic_(ic), instance_(instance) {}

        void                         commitText(const std::string& text) override;
        void                         forwardKey(EditKey key, bool release) override;
        void                         deleteSurrounding(int offset, unsigned int size) override;
        bool                         pressSystemKeys(int count) override;

        Surrounding                  surrounding() const override;
        Field                        field() const override;
        bool                         hasFocus() const override;

        void                         showPreedit(const std::string& text, bool underline) override;
        void                         clearPreedit() override;
        void                         resetPanel() override;
        void                         refreshPreedit() override;
        void                         refreshPanel() override;

        void                         showCandidates(const std::vector<std::string>& labels, int pageSize, std::function<void(size_t)> onPick) override;
        void                         hideCandidates() override;
        std::optional<CandidatePage> candidates() const override;
        void                         highlightCandidate(int index) override;
        void                         nextCandidatePage() override;
        void                         prevCandidatePage() override;
        void                         pickCandidate(int index) override;
        void                         setStatus(const std::string& text) override;

        std::unique_ptr<Timer>       startTimer(uint64_t deadlineUs, uint64_t accuracyUs, std::function<bool(Timer&)> onTime) override;

        std::string                  keyText(uint32_t sym) const override;

      private:
        std::shared_ptr<fcitx::CommonCandidateList> candidateList() const;

        fcitx::InputContext*                        ic_;
        fcitx::Instance*                            instance_;
    };

    class FcitxKeyPress final : public KeyPress {
      public:
        explicit FcitxKeyPress(fcitx::KeyEvent& event) : event_(event) {}

        uint32_t    sym() const override;
        uint32_t    states() const override;
        bool        isRelease() const override;
        bool        isModifier() const override;
        bool        isBareShift() const override;
        bool        hasModifier() const override;
        bool        isCursorMove() const override;
        std::string name() const override;
        void        replaceSym(uint32_t sym) override;
        void        accept() override;

      private:
        fcitx::KeyEvent& event_;
    };

} // namespace ngosen
