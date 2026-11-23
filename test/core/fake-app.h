/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-utf8.h"
#include "fake-host.h"
#include "ngosen-mode.h"

#include <cstdint>
#include <functional>
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
        int  bounceOnEditMs         = 0;     // bounces focus this long after a forwarded Backspace or a commit arrives, keeping both, as Brave on GNOME 51 does
        bool redrawsOnlyOnDeletion  = false; // redraws after deletions but not insertions, as a plain Draft.js box in Chrome on GNOME does
        bool dropsOldKeyPresses     = false; // drops a Backspace press not newer than the last one, as GLFW does under XIM
        bool hidesSelectionEnd      = false; // reports a selection as the cursor at its start, as Firefox does
        bool erasesOnPreedit        = false; // erases a selection when a preedit shows, as GTK 4 does, not on the commit
        int  commitLagMs            = 0;     // applies commits this late but typed keys at once, as Firefox does over D-Bus
        int  dropsCommitNumber      = 0;     // drops the nth non-empty commit, as voz.vn's editor in Brave drops one amid backspaces
        int  backspaceLagMs         = 0;     // applies forwarded backspaces this late but commits at once, as Antigravity's editor on GNOME sometimes does
        bool backspacesSkipLag      = false; // applies forwarded backspaces ahead of what lagMs holds back, as GNOME Shell's IBus bridge with forward-keys@ngosen.github.io does
        bool deletionsSkipLag       = false; // applies deletions through the text ahead of what lagMs holds back, as Firefox's snap does right after it opens
        bool hidesCursorMoves       = false; // reports no click and shows the cursor where the last edit left it, as GTK 3 on Plasma does
        bool dropsForwardedKeys     = false; // never gets keys the input method sends, as under GNOME Shell's IBus bridge
        bool erasesOnDel            = false; // erases the character before the cursor for each DEL committed, as a terminal does
        bool reportsTextOnceFilled  = false; // reports surrounding text only once the field has text, as Qt over IBus does
        bool announcesSelection     = false; // makes a selection the X11 primary selection, as Chromium on X11 does
        bool ignoresSurroundingDel  = false; // takes no deletion through its surrounding text, as WPS over D-Bus does
        bool reportsEmptyText       = false; // declares surrounding text but reports none and drops deletions through it, as Konsole does
        bool reportsTrailingNbsp    = false; // reports a space right before the cursor as U+00A0, as Firefox contenteditable does
        bool neverSendsText         = false; // declares surrounding text but never sends any, as WPS's own fcitx4 module does
        int  keyHoldMs              = 0;     // how long each typed key stays down before its release
        bool dropsForwardedHeldKey  = false; // drops a forwarded key while the user holds that key down, as GNOME Shell does
        // Shows its own copy of the text, which takes typed text and Backspace but no deletion through the
        // surrounding text, while it reports the browser's copy, which a Backspace leaves alone: Lark Sheets.
        bool keepsOwnText = false;
        // With keepsOwnText, a Backspace also changes the copy it reports, and the report holds a U+200B
        // after the cursor: Lark Base.
        bool backspacesShowInReport = false;
        // With keepsOwnText, empties the field it reports this long after each edit, as Univer's input box does.
        int emptiesAfterEditMs = 0;
        // Turns a deletion of characters into bytes on the text it reported last, as mutter does for
        // text-input-v3 and fcitx5 does on Wayland.
        bool             convertsDeleteOnReportedCopy = false;
        bool             reportsNothingAfterDelete    = false; // reports nothing after a deletion until a typed key, as Qt 6 over text-input-v3 does
        bool             repeatsOldTextAfterDelete    = false; // reports its text from before a deletion until a typed key, as Firefox on GNOME does
        int              keysBeforeNewReport          = 1;     // typed keys it takes to end repeatsOldTextAfterDelete
        bool             silentFromFirstDeletion      = false; // stops reporting at its first deletion through the surrounding text, for good
        bool             deletesOneTooMany            = false; // takes one more character than a deletion through the surrounding text asks for
        bool             deletesOneCharOnly           = false; // takes only the last character of a longer deletion, as a Lark chat box in Chromium
        bool             reportsTextAtChange          = false; // a late report shows the text as it was at that change, not as it is on arrival
        int              emptyCommitsBeforeReport     = 0;     // empty commits it takes to end repeatsOldTextAfterDelete, as Firefox on GNOME
        std::vector<int> ignoresRequestsOnEdits;               // these deletions get no report for empty commits, as Firefox on GNOME now and then
        bool             reportsOnlyWhenAsked      = false;    // after a deletion, reports typed keys only for an empty commit, as Firefox on Plasma now and then
        bool             keepsLineBreakAfterCommit = false;    // keeps a line break after a commit with the cursor past it until a typed key, as Ace in Firefox does
        bool             reportsOnFocus            = false;    // reports its field, empty or not, when it gets focus, as Firefox over GTK does
        bool             deletesAtReportedCursor   = false;    // deletes at the cursor of its last report and leaves the cursor there, as Firefox over GTK does
        bool             dropsSurroundingOnDelete  = false;    // stops declaring surrounding after a deletion through it until a key or click, as Firefox over fcitx5-gtk does
        int              reportsOnEdit             = 0;        // the nth deletion is reported anyway, as Qt now and then
        bool             showsEnter                = false;    // shows each Enter, typed or forwarded, as a line break, and Shift+Enter as U+2028
        bool             ctrlBackSpaceDeletesWord  = false;    // a BackSpace forwarded before the app saw Ctrl go up deletes the word, as in Chrome
        bool             showsTab                  = false;    // shows each Tab key as ⇥ and Shift+Tab as ⇤, unlike a tab character in a commit
        // Applies deletions and commits once per main-loop turn, keeping only the last of each, as a
        // client behind mutter's text-input-v3 does; a key let through ends the turn first.
        bool keepsLastEditPerTurn = false;
        int  turnMs               = 0; // how long such a turn stays open
        // Its cursor stays past the real one after a deletion and a commit in one turn, as Firefox's does,
        // until a typed key corrects it.
        bool cursorStaysPastEdit = false;
        // The first commit whose turn ends this soon after a deletion's leaves the cursor past by the
        // deletion, as Firefox does when the page is busy, until a typed key or another commit moves it.
        int appliesDeletionAfterMs = 0;
        int oldTextAfterDeleteMs   = 0; // for this long after a deletion its reports show the text from before it, as Firefox on GNOME sometimes does
        // Reports the cursor moving over a deletion and its commit on the old text before the new text, as
        // Slate in Firefox on Plasma does.
        bool movesCursorBeforeText = false;
        // Sends typed keys to a terminal it draws and keeps only commits in the field it reports, and sends a
        // commit made outside a composition once more with the text since the last composition started, as
        // xterm.js does in WebKitGTK. A commit sent before WebKitGTK has taken its preedit starts none.
        bool keepsOnlyCommits = false;
        // With keepsOnlyCommits, sends its last commit to the terminal again on an empty commit, as xterm.js in
        // VaultID does behind GNOME Shell.
        bool resendsOnEmptyCommit = false;
        // With movesCursorBeforeText, shows the deletion first with the cursor still past the commit, as
        // CKEditor in Firefox on Plasma does.
        bool        showsDeletionWithCursorPast = false;
        std::string textAfterCursor; // text the field starts with, after the cursor
        // Leaves focus and comes back reporting an empty field for a moment after each forwarded Backspace
        // and each edit, as the voz.vn composer does in Brave on Plasma Wayland.
        bool bouncesEmptyAfterEdit = false;
        // Applies a commit only after the forwarded Backspaces sent before it, and loses it when there were
        // two or more, as the voz.vn composer does in Brave on Plasma Wayland. Needs keyStepMs.
        bool losesCommitAmidBackspaces = false;
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
        FakeApp(FakeLoop& loop, Field field, AppQuirks quirks) : loop_(loop), field_(std::move(field)), quirks_(quirks) {
            text_ = utf8::decode(quirks_.textAfterCursor);
        }

        void attach(TypingState* state) {
            state_ = state;
        }
        // Lets a test hold up one stretch of reports, as an app busy for a moment does.
        void setReportDelay(int ms) {
            quirks_.reportDelayMs = ms;
        }
        // Lets a test end a stretch of dropped deletions, as GNOME Shell does once the machine is idle again.
        void setIgnoresSurroundingDel(bool ignores) {
            quirks_.ignoresSurroundingDel = ignores;
        }
        // Shows a changed hint this late, after keys sent at the same time, as GNOME Shell does for Chrome.
        void setHintLag(int ms) {
            hintLagMs_ = ms;
        }
        // Lets a test slow down how fast the app applies edits for a stretch, as a starved app does.
        void setLag(int ms) {
            quirks_.lagMs = ms;
        }
        // Renames the app with no focus change, as GNOME Shell does when it names the app late.
        void setProgram(const std::string& program) {
            field_.program = program;
        }
        // Presses and releases a key; the app inserts it when the input method lets it through.
        void        type(uint32_t sym);
        std::string text() const;
        // Moves the cursor without an edit, as a click does.
        void moveCursor(size_t to);
        // Adds a copy of the text after the cursor and shows the cursor at its start, as VS Code's editor
        // on GNOME does for the first letter of a line.
        void copyTextAfterStartCursor();
        // Selects from anchor to cursor, as Shift+arrows or a double click do; typing replaces it.
        void select(size_t anchor, size_t cursor);
        // Leaves focus for ms and takes it back, as WPS does when it opens a window on a double click.
        void leaveFocus(int ms);
        void bounceReportingEmpty(int ms);
        // Commits the preedit itself and resets, as an IBus client does with a preedit sent to be committed;
        // this models xterm.js only.
        void resetCommittingPreedit();
        // True once xterm.js has taken the preedit into a composition.
        bool composesPreedit() const {
            return composing_ && !shownPreedit_.empty();
        }
        const std::string& shownPreedit() const {
            return shownPreedit_;
        }
        // Leaves focus and comes back after ms with text put at the cursor meanwhile, as a key typed right
        // after Alt+Tab does when both its press and release reach the window before focus-in.
        void returnWithText(const std::string& typed, int ms);
        // The input method sees the key in the window it is leaving, the letter lands here, and focus comes
        // back within ms, as GNOME Shell's IBus bridge does for a key typed right after Alt+Tab.
        void gnomeReturnWithKey(uint32_t sym, int ms);
        // Moves within ms to an empty field of the same page that keeps its own text, as a click from a
        // page's search box into a Lark sheet does.
        void switchToOwnTextField(int ms);
        // Leaves focus for ms and comes back to the same editor, which still shows the last cell until the
        // next typed key replaces it, as a click on another cell in Luckysheet does.
        void clickCellOpenedByKey(int ms);
        // Loses focus and gets it back within ms in a new empty field, with no reset in between, as a
        // browser closed and opened again does under GNOME Shell.
        void focusNewEmptyField(ngosen::Mode mode, int ms);
        // Leaves the field, then within ms focuses a new one that holds text before the cursor.
        void focusNewFieldWith(const std::string& text, int ms);
        // Puts text around the cursor with no key through the input method, as a paste does.
        void pageInserts(const std::string& before, const std::string& after);
        // The field now holds only this text, cursor at its end, with no key: another cell in the same context.
        void pageShowsCell(const std::string& text);
        // A key whose press reached the page before the field had focus, as the key that opens a sheet's
        // cell editor: the page puts text around the cursor, and the input method gets only the release.
        void pageTakesKey(uint32_t sym, const std::string& before, const std::string& after);
        // Like pageTakesKey, but the field reports the letter only with its next change, as AG Grid does.
        void pageTakesKeyUnreported(uint32_t sym, const std::string& letter);
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
        // The text shown after the cursor that is never committed.
        const std::string& hint() const {
            return hint_;
        }
        // Commits, deletions and forwarded keys sent while the hint showed.
        int editsUnderHint() const {
            return editsUnderHint_;
        }
        // Candidate lists shown between a replacement's first forwarded backspace and its commit.
        int panelsMidReplacement() const {
            return panelsMidReplacement_;
        }
        // The longest time from forwarding a replacement's first backspace to its commit.
        int slowestCommitMs() const {
            return slowestCommitMs_;
        }
        int shortcuts() const {
            return shortcuts_;
        }

        void        commitText(const std::string& text) override;
        void        forwardKey(EditKey key, bool release, uint32_t time) override;
        void        forwardKeyPress(uint32_t sym, uint32_t states, uint32_t code, bool release, uint32_t time) override;
        void        deleteSurrounding(int offset, unsigned int size) override;
        bool        pressSystemKeys(int count) override;

        Surrounding surrounding() const override;
        Field       field() const override {
            Field field = field_;
            if (quirks_.reportsTextOnceFilled && text_.empty())
                field.surroundingText = false;
            if (surroundingDropped_)
                field.surroundingText = false;
            return field;
        }
        bool hasFocus() const override {
            return focused_;
        }
        std::optional<std::string> takeSelection() override {
            return std::exchange(announced_, std::nullopt);
        }
        std::string primarySelection() const override {
            return announced_.value_or("");
        }

        void showPreedit(const std::string& text, bool /*underline*/) override;
        void clearPreedit() override;
        void showHint(const std::string& text) override {
            if (hintLagMs_ > 0)
                later(hintLagMs_, [this, text] { hint_ = text; });
            else
                hint_ = text;
        }
        // fcitx5 drops the candidate list along with the rest of the panel.
        void resetPanel() override {
            hideCandidates();
        }
        void refreshPreedit() override {}
        void refreshPanel() override {}

        void showCandidates(const std::vector<std::string>& labels, int /*pageSize*/, std::function<void(size_t)> onPick, CandidateLayout layout) override {
            if (forwardedAt_ > 0)
                ++panelsMidReplacement_;
            labels_      = labels;
            onPick_      = std::move(onPick);
            inRow_       = layout == CandidateLayout::Row;
            highlighted_ = inRow_ ? -1 : 0;
        }
        bool labelsInRow() const {
            return inRow_;
        }
        void hideCandidates() override {
            if (!labels_.empty())
                ++candidatesHidden_;
            labels_.clear();
            onPick_ = nullptr;
        }
        std::optional<CandidatePage> candidates() const override {
            if (labels_.empty())
                return std::nullopt;
            return CandidatePage{.total = static_cast<int>(labels_.size())};
        }
        void highlightCandidate(int index) override {
            highlighted_ = index;
        }
        int highlighted() const {
            return highlighted_;
        }
        void nextCandidatePage() override {}
        void prevCandidatePage() override {}
        // A click on an item of the list.
        void pickCandidate(int index) override {
            if (onPick_ && index >= 0 && static_cast<size_t>(index) < labels_.size())
                onPick_(static_cast<size_t>(index));
        }
        // How many times a shown candidate list was taken away.
        int candidatesHidden() const {
            return candidatesHidden_;
        }
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
        int                                 highlighted_      = 0;
        int                                 candidatesHidden_ = 0;
        bool                                reportsEmptyOnce_ = false;
        bool                                inRow_            = false;
        std::function<void(size_t)>         onPick_;
        void                                insert(const std::u32string& text, bool typed = false);
        void                                noteTypedKey();
        void                                goSilentAfterDelete();
        bool                                eraseSelection();
        void                                eraseBefore(size_t count);
        void                                eraseAround(int offset, unsigned int size);
        bool                                leaveIfEmptied();
        size_t                              reportedBytesBefore(unsigned int size) const;
        unsigned int                        charsForBytesBefore(size_t bytes) const;
        void                                changed();
        void                                bounceAfterEdit();
        void                                redrawAfterEdit();
        void                                bounceEmptyAfterEdit();
        void                                later(int ms, std::function<void()> action);
        void                                startTurn();
        void                                endTurn();
        void                                reportMovedCursor(const std::u32string& text, size_t cursor);
        void                                applyHeldEdit(const std::string& text);
        bool                                applyFieldKey(uint32_t sym, uint32_t state);

        FakeLoop&                           loop_;
        Field                               field_;
        AppQuirks                           quirks_;
        TypingState*                        state_          = nullptr;
        int                                 queuedKeys_     = 0;
        int                                 redraws_        = 0;
        int                                 backspaceBurst_ = 0; // forwarded Backspaces queued since the queue was last empty
        std::u32string                      heldCommit_;         // commits waiting for queued Backspaces
        bool                                emptyBouncePending_ = false;
        std::u32string                      ownText_;
        bool                                inReport_        = false;
        bool                                focused_         = true;
        uint64_t                            forwardedAt_     = 0;
        uint32_t                            lastBackSpaceAt_ = 0;
        uint32_t                            heldSym_         = 0;
        int                                 slowestCommitMs_ = 0;
        int                                 shortcuts_       = 0;

        int                                 panelsMidReplacement_ = 0;
        std::u32string                      text_;
        std::optional<std::string>          announced_;
        size_t                              cursor_      = 0;
        size_t                              anchor_      = 0;
        size_t                              shownCursor_ = 0;
        size_t                              shownAnchor_ = 0;
        std::vector<std::string>            labels_;
        std::string                         status_;
        std::string                         hint_;
        int                                 editsUnderHint_ = 0;
        int                                 hintLagMs_      = 0;
        std::u32string                      previousText_;
        size_t                              previousCursor_ = 0;
        std::u32string                      reportedText_;
        size_t                              reportedCursor_ = 0;
        std::u32string                      oldText_;
        size_t                              oldCursor_          = 0;
        uint64_t                            oldTextUntilUs_     = 0;
        size_t                              reportedAnchor_     = 0;
        bool                                silentAfterDelete_  = false;
        bool                                surroundingDropped_ = false;
        int                                 keysSinceDelete_    = 0;
        bool                                turnPending_        = false;
        std::optional<size_t>               turnDeleteBytes_;
        std::optional<std::string>          turnCommit_;
        size_t                              cursorPastBy_      = 0;
        uint64_t                            deletionTurnAtUs_  = 0;
        size_t                              deletionTurnChars_ = 0;
        std::optional<unsigned int>         heldDelete_;
        bool                                showsMovedCursor_ = false;
        bool                                holdsUnreported_  = false;
        bool                                cellOpensOnKey_   = false;
        bool                                ctrlDown_         = false;
        int                                 emptyCommits_     = 0;
        int                                 commits_          = 0; ///< non-empty commits that reached the app
        int                                 deletions_        = 0;
        Path                                sent_{.surroundingDeletes = 0, .forwardedBackSpaces = 0, .systemKeyPresses = 0, .preedits = 0};
        std::vector<std::unique_ptr<Timer>> timers_;

        // With keepsOnlyCommits: the composition xterm.js tracks, and a preedit that has not started one yet.
        bool           composing_ = false;
        std::u32string lastCommit_;
        size_t         compositionStart_ = 0;
        std::string    shownPreedit_;
        bool           preeditWaits_           = false;
        bool           clearsPreeditAtTurnEnd_ = false;
        void           startComposition();
    };

    // The text the keys give in Preedit mode, where the app only ever gets finished text.
    // reportsText: the field reports its text, which Space needs to fix a kept word.
    std::string telexOutput(const std::vector<uint32_t>& keys, bool reportsText = false);
    // The same with the given settings and macro table.
    std::string telexOutput(FakeResources& resources, const std::vector<uint32_t>& keys, bool reportsText = false);

    // What the app shows after typing, and what the typing logic sent it to get there.
    struct Typed {
        std::string text;
        Path        sent;
        int         shortcuts = 0; // Ctrl or Alt keys that reached the app
    };

    // The ways what was sent departs from the expected path, empty when they match.
    std::vector<std::string> pathMismatches(const Path& sent, const Path& expected);

    Typed                    typeInAppTraced(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs);

    // Or this into a key for typeInApp to press it with Shift held.
    inline constexpr uint32_t WithShift = 1U << 30;
    // Or this in to press it with Ctrl held.
    inline constexpr uint32_t WithCtrl = 1U << 29;

    // The app's text after typing the keys in Gõ Sen mode, waiting gapsMs[i] after key i.
    std::string typeInApp(const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs = nullptr);
    // The same with the given settings and macro table.
    std::string typeInApp(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs,
                          int* slowestCommitMs = nullptr);

} // namespace ngosen::test
