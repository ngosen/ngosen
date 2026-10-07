/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"

#include <memory>

namespace fcitx {
    class CommonCandidateList;
    class InputContext;
}

namespace ngosen {

    class FcitxHost final : public Host {
      public:
        explicit FcitxHost(fcitx::InputContext* ic) : ic_(ic) {}

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

      private:
        std::shared_ptr<fcitx::CommonCandidateList> candidateList() const;

        fcitx::InputContext*                        ic_;
    };

} // namespace ngosen
