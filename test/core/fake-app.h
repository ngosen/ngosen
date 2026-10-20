/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "fake-host.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ngosen {
    class TypingState;
}

namespace ngosen::test {

    class FakeResources;

    // Ways a real app's field departs from an ideal one.
    struct AppQuirks {
        bool reports                = true; // tells the input method when its text changes
        int  reportDelayMs          = 0;
        bool staleReport            = false; // reports the text as it was one change earlier
        int  lagMs                  = 0;     // applies commits, deletions and keys this late, in order
        bool handsBackKeys          = false; // sends forwarded keys back to the input method first, as XIM clients do
        int  keyStepMs              = 0;     // applies forwarded backspaces one per step, reporting after each
        int  switchBusyMs           = 0;     // drops forwarded keys this long after a switch key, as Qt does
        bool dropsCommitsInReport   = false; // drops commits sent while its report is handled, as wlroots does
        int  refocusWhenEmptiedMs   = 0;     // leaves focus this long when its text becomes empty, as Zed does
        bool resendsKeysWithoutText = false; // sends a key that types no text back once more, marked as repeated, as gnome-text-editor does
        bool takesXTest             = false; // gets backspaces as XTEST presses, which come back to the input method first
        int  redrawAfterEditMs      = 0;     // bounces focus this long after each edit and drops commits until then, as TikTok in Chrome does
        bool redrawsOnlyOnDeletion  = false; // redraws after deletions but not insertions, as a plain Draft.js box in Chrome on GNOME does
    };

    // An app field that keeps its own text, so a test can compare what the user would see with what
    // was typed.
    class FakeApp final : public Host {
      public:
        FakeApp(FakeLoop& loop, Field field, AppQuirks quirks) : loop_(loop), field_(std::move(field)), quirks_(quirks) {}

        void attach(TypingState* state) {
            state_ = state;
        }
        // Presses and releases a key; the app inserts it when the input method lets it through.
        void type(uint32_t sym);
        // The user pressed the key that switches input methods.
        void        pressSwitchKey();
        std::string text() const;
        // The longest time from forwarding a replacement's first backspace to its commit.
        int slowestCommitMs() const {
            return slowestCommitMs_;
        }

        void        commitText(const std::string& text) override;
        void        forwardKey(EditKey key, bool release) override;
        void        passKey(uint32_t sym, uint32_t states, uint32_t code) override;
        void        deleteSurrounding(int offset, unsigned int size) override;
        bool        pressSystemKeys(int count) override;

        Surrounding surrounding() const override;
        Field       field() const override {
            return field_;
        }
        bool hasFocus() const override {
            return focused_;
        }

        void                         showPreedit(const std::string& /*text*/, bool /*underline*/) override {}
        void                         clearPreedit() override {}
        void                         resetPanel() override {}
        void                         refreshPreedit() override {}
        void                         refreshPanel() override {}

        void                         showCandidates(const std::vector<std::string>& /*labels*/, int /*pageSize*/, std::function<void(size_t)> /*onPick*/) override {}
        void                         hideCandidates() override {}
        std::optional<CandidatePage> candidates() const override {
            return std::nullopt;
        }
        void        highlightCandidate(int /*index*/) override {}
        void        nextCandidatePage() override {}
        void        prevCandidatePage() override {}
        void        pickCandidate(int /*index*/) override {}
        void        setStatus(const std::string& /*text*/) override {}
        std::string translate(const char* text) const override {
            return text;
        }

        std::unique_ptr<Timer> startTimer(uint64_t deadlineUs, uint64_t /*accuracyUs*/, std::function<bool(Timer&)> onTime) override {
            return loop_.add(deadlineUs, std::move(onTime));
        }

        std::string keyText(uint32_t sym) const override;

      private:
        void                                insert(const std::u32string& text);
        void                                eraseBefore(size_t count);
        void                                eraseAround(int offset, unsigned int size);
        void                                changed();
        void                                redrawAfterEdit();
        void                                later(int ms, std::function<void()> action);

        FakeLoop&                           loop_;
        Field                               field_;
        AppQuirks                           quirks_;
        TypingState*                        state_           = nullptr;
        int                                 queuedKeys_      = 0;
        int                                 redraws_         = 0;
        bool                                inReport_        = false;
        bool                                focused_         = true;
        uint64_t                            busyUntilUs_     = 0;
        uint64_t                            forwardedAt_     = 0;
        int                                 slowestCommitMs_ = 0;
        std::u32string                      text_;
        size_t                              cursor_ = 0;
        std::u32string                      previousText_;
        size_t                              previousCursor_ = 0;
        std::vector<std::unique_ptr<Timer>> timers_;
    };

    // The text the keys give in Preedit mode, where the app only ever gets finished text.
    std::string telexOutput(const std::vector<uint32_t>& keys);

    // The app's text after typing the keys in Gõ Sen mode, waiting gapsMs[i] after key i.
    std::string typeInApp(const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs = nullptr);
    // The same with the given settings and macro table.
    std::string typeInApp(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs,
                          int* slowestCommitMs = nullptr);

} // namespace ngosen::test
