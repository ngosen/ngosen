/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "fake-host.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
        bool dropsCommitsInReport   = false; // drops commits sent while its report is handled, as wlroots does
        int  refocusWhenEmptiedMs   = 0;     // leaves focus this long when its text becomes empty, as Zed does
        bool resendsKeysWithoutText = false; // sends a key that types no text back once more, marked as repeated, as gnome-text-editor does
        bool takesXTest             = false; // gets backspaces as XTEST presses, which come back to the input method first
        int  redrawAfterEditMs      = 0;     // bounces focus this long after each edit and drops commits until then, as TikTok in Chrome does
        bool redrawsOnlyOnDeletion  = false; // redraws after deletions but not insertions, as a plain Draft.js box in Chrome on GNOME does
        bool dropsOldKeyPresses     = false; // drops a Backspace press not newer than the last one, as GLFW does under XIM
        bool hidesSelectionEnd      = false; // reports a selection as the cursor at its start, as Firefox does
        int  commitLagMs            = 0;     // applies commits this late but typed keys at once, as Firefox does over D-Bus
        int  backspaceLagMs         = 0;     // applies forwarded backspaces this late but commits at once, as Antigravity's editor on GNOME sometimes does
        bool hidesCursorMoves       = false; // reports no click and shows the cursor where the last edit left it, as GTK 3 on Plasma does
        bool dropsForwardedKeys     = false; // never gets keys the input method sends, as under GNOME Shell's IBus bridge
        bool erasesOnDel            = false; // erases the character before the cursor for each DEL committed, as a terminal does
        bool reportsTextOnceFilled  = false; // reports surrounding text only once the field has text, as Qt over IBus does
        bool announcesSelection     = false; // makes a selection the X11 primary selection, as Chromium on X11 does
        bool ignoresSurroundingDel  = false; // takes no deletion through its surrounding text, as WPS over D-Bus does
        bool reportsEmptyText       = false; // declares surrounding text but reports none and drops deletions through it, as Konsole does
        // Turns a deletion of characters into bytes on the text it reported last, as mutter does for
        // text-input-v3 and fcitx5 does on Wayland.
        bool convertsDeleteOnReportedCopy = false;
        bool reportsNothingAfterDelete    = false; // reports nothing after a deletion until a typed key, as Qt 6 over text-input-v3 does
        bool repeatsOldTextAfterDelete    = false; // reports its text from before a deletion until a typed key, as Firefox on GNOME does
        bool showsEnter                   = false; // shows each Enter, typed or forwarded, as a line break, and Shift+Enter as U+2028
        bool showsTab                     = false; // shows each Tab key as ⇥ and Shift+Tab as ⇤, unlike a tab character in a commit
    };

    // An app field that keeps its own text, so a test can compare what the user would see with what
    // was typed.
    // How the typing logic changes the app's text; -1 means not checked.
    struct Path {
        int surroundingDeletes  = -1;
        int forwardedBackSpaces = -1; // presses only
        int systemKeyPresses    = -1; // keys pressed, not calls
        int preedits            = -1;
    };

    class FakeApp final : public Host {
      public:
        FakeApp(FakeLoop& loop, Field field, AppQuirks quirks) : loop_(loop), field_(std::move(field)), quirks_(quirks) {}

        void attach(TypingState* state) {
            state_ = state;
        }
        // Presses and releases a key; the app inserts it when the input method lets it through.
        void        type(uint32_t sym);
        std::string text() const;
        // Moves the cursor without an edit, as a click does.
        void moveCursor(size_t to);
        // Selects from anchor to cursor, as Shift+arrows or a double click do; typing replaces it.
        void select(size_t anchor, size_t cursor);
        // Adds text after the cursor with no key, as a browser's autofill does, selected or not.
        void suggest(const std::string& tail, bool selected);
        // How the typing logic asked the app to change its text so far.
        const Path& sent() const {
            return sent_;
        }
        const std::vector<std::string>& labels() const {
            return labels_;
        }
        const std::string& status() const {
            return status_;
        }
        // The longest time from forwarding a replacement's first backspace to its commit.
        int slowestCommitMs() const {
            return slowestCommitMs_;
        }

        void        commitText(const std::string& text) override;
        void        forwardKey(EditKey key, bool release, uint32_t time) override;
        void        deleteSurrounding(int offset, unsigned int size) override;
        bool        pressSystemKeys(int count) override;

        Surrounding surrounding() const override;
        Field       field() const override {
            Field field = field_;
            if (quirks_.reportsTextOnceFilled && text_.empty())
                field.surroundingText = false;
            return field;
        }
        bool hasFocus() const override {
            return focused_;
        }
        std::optional<std::string> takeSelection() override {
            return std::exchange(announced_, std::nullopt);
        }

        void showPreedit(const std::string& text, bool /*underline*/) override {
            if (!text.empty())
                ++sent_.preedits;
        }
        void clearPreedit() override {}
        void resetPanel() override {}
        void refreshPreedit() override {}
        void refreshPanel() override {}

        void showCandidates(const std::vector<std::string>& labels, int /*pageSize*/, std::function<void(size_t)> /*onPick*/) override {
            labels_ = labels;
        }
        void hideCandidates() override {
            labels_.clear();
        }
        std::optional<CandidatePage> candidates() const override {
            if (labels_.empty())
                return std::nullopt;
            return CandidatePage{.total = static_cast<int>(labels_.size())};
        }
        void highlightCandidate(int /*index*/) override {}
        void nextCandidatePage() override {}
        void prevCandidatePage() override {}
        void pickCandidate(int /*index*/) override {}
        void setStatus(const std::string& text) override {
            status_ = text;
        }
        std::string translate(const char* text) const override {
            return text;
        }

        std::unique_ptr<Timer> startTimer(uint64_t deadlineUs, uint64_t /*accuracyUs*/, std::function<bool(Timer&)> onTime) override {
            return loop_.add(deadlineUs, std::move(onTime));
        }

        std::string keyText(uint32_t sym) const override;

      private:
        void                                insert(const std::u32string& text, bool typed = false);
        bool                                eraseSelection();
        void                                eraseBefore(size_t count);
        void                                eraseAround(int offset, unsigned int size);
        bool                                leaveIfEmptied();
        size_t                              reportedBytesBefore(unsigned int size) const;
        unsigned int                        charsForBytesBefore(size_t bytes) const;
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
        uint64_t                            forwardedAt_     = 0;
        uint32_t                            lastBackSpaceAt_ = 0;
        int                                 slowestCommitMs_ = 0;
        std::u32string                      text_;
        std::optional<std::string>          announced_;
        size_t                              cursor_      = 0;
        size_t                              anchor_      = 0;
        size_t                              shownCursor_ = 0;
        size_t                              shownAnchor_ = 0;
        std::vector<std::string>            labels_;
        std::string                         status_;
        std::u32string                      previousText_;
        size_t                              previousCursor_ = 0;
        std::u32string                      reportedText_;
        size_t                              reportedCursor_    = 0;
        size_t                              reportedAnchor_    = 0;
        bool                                silentAfterDelete_ = false;
        Path                                sent_{.surroundingDeletes = 0, .forwardedBackSpaces = 0, .systemKeyPresses = 0, .preedits = 0};
        std::vector<std::unique_ptr<Timer>> timers_;
    };

    // The text the keys give in Preedit mode, where the app only ever gets finished text.
    std::string telexOutput(const std::vector<uint32_t>& keys);
    // The same with the given settings and macro table.
    std::string telexOutput(FakeResources& resources, const std::vector<uint32_t>& keys);

    // What the app shows after typing, and what the typing logic sent it to get there.
    struct Typed {
        std::string text;
        Path        sent;
    };

    // The ways what was sent departs from the expected path, empty when they match.
    std::vector<std::string> pathMismatches(const Path& sent, const Path& expected);

    Typed                    typeInAppTraced(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs);

    // Or this into a key for typeInApp to press it with Shift held.
    inline constexpr uint32_t WithShift = 1U << 30;

    // The app's text after typing the keys in Gõ Sen mode, waiting gapsMs[i] after key i.
    std::string typeInApp(const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs = nullptr);
    // The same with the given settings and macro table.
    std::string typeInApp(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs,
                          int* slowestCommitMs = nullptr);

} // namespace ngosen::test
