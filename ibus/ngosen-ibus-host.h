/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"
#include "ngosen-key.h"

#include <functional>
#include <ibus.h>
#include <memory>
#include <string>

namespace ngosen {

    class IBusHost final : public Host {
      public:
        explicit IBusHost(IBusEngine* engine) : engine_(engine) {}

        // IBus tells the engine about the field piece by piece; the engine passes each piece on.
        void setFocus(bool focus) {
            focus_ = focus;
        }
        void setClient(std::string client) {
            client_ = std::move(client);
        }
        void setPurpose(guint purpose) {
            purpose_ = purpose;
        }
        void setOnCommit(std::function<void(const std::string&)> onCommit) {
            onCommit_ = std::move(onCommit);
        }

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
        std::string                  translate(const char* text) const override;

        std::unique_ptr<Timer>       startTimer(uint64_t deadlineUs, uint64_t accuracyUs, std::function<bool(Timer&)> onTime) override;

        std::string                  keyText(uint32_t sym) const override;

      private:
        IBusEngine*                             engine_;
        bool                                    focus_   = false;
        guint                                   purpose_ = IBUS_INPUT_PURPOSE_FREE_FORM;
        std::string                             client_;
        std::string                             preedit_;
        bool                                    underline_ = false;
        std::string                             status_;
        std::function<void(const std::string&)> onCommit_;
    };

    class IBusKeyPress final : public KeyPress {
      public:
        IBusKeyPress(guint keyval, guint keycode, guint state) : sym_(keyval), appSym_(keyval), code_(keycode), state_(state) {}

        uint32_t    sym() const override;
        uint32_t    states() const override;
        bool        isRelease() const override;
        bool        isModifier() const override;
        bool        isBareShift() const override;
        uint32_t    code() const override;
        uint32_t    time() const override;
        bool        hasModifier() const override;
        bool        isCursorMove() const override;
        std::string name() const override;
        void        replaceSym(uint32_t sym) override;
        void        accept() override;

        bool        accepted() const {
            return accepted_;
        }
        // The symbol the app should get instead of the one pressed, or 0 when unchanged.
        uint32_t replacedSym() const {
            return appSym_ != sym_ ? appSym_ : 0;
        }

      private:
        uint32_t sym_;
        uint32_t appSym_;
        uint32_t code_;
        uint32_t state_;
        bool     accepted_ = false;
    };

    // Calls onTime from the GLib main loop at deadlineUs on g_get_monotonic_time()'s clock, which is
    // CLOCK_MONOTONIC.
    std::unique_ptr<Timer> startGlibTimer(uint64_t deadlineUs, std::function<bool(Timer&)> onTime);

} // namespace ngosen
