/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-state.h"
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-fcitx-host.h"
#include "ngosen-log.h"
#include "ngosen-utf8.h"
#include "ngosen-xtest.h"
#include "lotus.h"

#include <cstddef>
#include <fcitx-utils/log.h>
#include <fcitx/inputpanel.h>
#include <fcitx/menu.h>
#include <fcitx/userinterface.h>

#include <algorithm>
#include <string>

#include <thread>

namespace fcitx {
    constexpr int MAX_SCAN_LENGTH = 15;
    // XIM, IBus and D-Bus clients queue forwarded keys, and XIM may hand one back; without a
    // surrounding text report the commit waits this long for them.
    constexpr uint64_t ForwardWaitUs = 15000;
    // Chromium asks the input method about each key before it handles the key, so our Shift release
    // coming back does not mean the Left presses before it have moved the selection yet.
    constexpr uint64_t XTestSelectSettleUs = 50000;
    // Gives up on a selection whose Shift release never comes back.
    constexpr uint64_t XTestSelectTimeoutUs = 500000;

    static inline bool isWordBreak(uint32_t ucs4) {
        // Space, tab, newline, carriage return, null, or punctuation/symbols (: ; < = > ? @)
        return ucs4 == ' ' || ucs4 == '\t' || ucs4 == '\n' || ucs4 == '\r' || ucs4 == 0 || (ucs4 >= 58 && ucs4 <= 64);
    }

    LotusState::LotusState(LotusEngine* engine, InputContext* ic) : engine_(engine), ic_(ic), host_(std::make_unique<ngosen::FcitxHost>(ic, engine->instance())) {
        setEngine();
    }

    void LotusState::setEngine() {
        lotusEngine_.reset();

        if (engine_->options().inputMethod == "Custom") {
            const auto&        keymaps = *engine_->customKeymap().customKeymap;
            std::vector<char*> charArray;
            charArray.reserve((keymaps.size() * 2) + 1);
            for (const auto& keymap : keymaps) {
                charArray.push_back(const_cast<char*>(keymap.key->data()));   //NOLINT
                charArray.push_back(const_cast<char*>(keymap.value->data())); //NOLINT
            }
            charArray.push_back(nullptr);
            lotusEngine_.reset(NewCustomEngine(charArray.data(), engine_->dictionary(), engine_->macroTable()));
        } else {
            lotusEngine_.reset(NewEngine(engine_->options().inputMethod.data(), engine_->dictionary(), engine_->macroTable()));
        }
        setOption();
        resetMacroSkip();
    }

    void LotusState::setOption() {
        if (!lotusEngine_)
            return;
        FcitxBambooEngineOption option = {
            .autoNonVnRestore    = engine_->options().autoNonVnRestore,
            .ddFreeStyle         = engine_->options().ddFreeStyle,
            .macroEnabled        = engine_->options().enableMacro,
            .autoCapitalizeMacro = engine_->options().capitalizeMacro,
            .spellCheckWithDicts = engine_->options().spellCheck,
            .outputCharset       = engine_->options().outputCharset.data(),
            .modernStyle         = engine_->options().modernStyle,
            .freeMarking         = engine_->options().freeMarking,
            .w2u                 = static_cast<int>(engine_->options().w2u),
            .bracketTransform    = static_cast<int>(engine_->options().bracketTransform),
            .timeFormat          = engine_->options().timeFormat.data(),
            .dateFormat          = engine_->options().dateFormat.data(),
        };

        EngineSetOption(lotusEngine_.handle(), &option);
    }

    void LotusState::sendBackspaceKeys(int count) const {
        if (!host_->pressSystemKeys(count)) {
            NGOSEN_ERROR("Cannot send backspaces: XTEST is unavailable");
        }
    }

    namespace {
        // Firefox's address bar sets no Url flag and reports its autofill suffix as unselected, so
        // recognise it by shape: one line, and only URL characters after the cursor.
        bool isUrlChar(uint32_t c) {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                return true;
            }
            switch (c) {
                case '-':
                case '.':
                case '_':
                case '~':
                case ':':
                case '/':
                case '?':
                case '#':
                case '[':
                case ']':
                case '@':
                case '!':
                case '$':
                case '&':
                case '\'':
                case '(':
                case ')':
                case '*':
                case '+':
                case ',':
                case ';':
                case '=':
                case '%': return true;
                default: return false;
            }
        }

        bool textAfterCursorLooksLikeUrl(const ngosen::Surrounding& s) {
            if (!s.isValid() || s.cursor() != s.anchor()) {
                return false;
            }
            const unsigned cursor = s.cursor();
            unsigned       i      = 0;
            size_t         after  = 0;
            for (char32_t c : ngosen::utf8::decode(s.text())) {
                if (c == U'\n') {
                    return false;
                }
                if (i >= cursor) {
                    if (!isUrlChar(c)) {
                        return false;
                    }
                    ++after;
                }
                ++i;
            }
            return after > 0;
        }
    } // namespace

    namespace {
        // The word being typed starts the field, so an extra backspace for a wrong autofill guess
        // deletes nothing. realtextLen is stale here after the address bar is cleared.
        bool onlyCurrentWordBeforeCursor(const ngosen::Surrounding& s, const std::string& buff) {
            if (!s.isValid() || buff.empty() || s.cursor() != ngosen::utf8::length(buff)) {
                return false;
            }
            const std::string& t = s.text();
            return t.size() > buff.size() && t.compare(0, buff.size(), buff) == 0;
        }
    } // namespace

    bool LotusState::isAutofillCertain(const ngosen::Surrounding& s) {
        if (!s.isValid() || oldPreBuffer_.empty()) {
            return false;
        }

        const std::u32string u32Text         = ngosen::utf8::decode(s.text());
        const std::u32string u32OldPreBuffer = ngosen::utf8::decode(oldPreBuffer_);

        const unsigned int   cursor  = s.cursor();
        const unsigned int   anchor  = s.anchor();
        const size_t         textLen = u32Text.length();

        // Fix that surrounding text is delay update
        const size_t buffLen    = u32OldPreBuffer.length();
        const size_t pb         = u32Text.find(u32OldPreBuffer);
        size_t       rangeStart = static_cast<size_t>(cursor) >= buffLen ? static_cast<size_t>(cursor) - buffLen : 0;
        const bool   sameprefix = pb != std::u32string::npos && pb >= rangeStart && pb <= static_cast<size_t>(cursor);

        // Detect browser autofill/autocomplete suggestions via selection.
        if (cursor != anchor) {
            unsigned int selectionStart = std::min(anchor, cursor);
            unsigned int selectionEnd   = std::max(anchor, cursor);

            // Only consider it browser autofill if the selection starts at the cursor
            // and extends to the end of the line (common address bar behavior).
            if (selectionStart >= cursor || (selectionStart < cursor && selectionEnd > cursor)) {
                if (!sameprefix)
                    return false;
                // If the selection contains a newline, it's likely a multiline editor (AI ghost text),
                // not a single-line URL/Search bar.
                size_t p = u32Text.find(U'\n', selectionStart);
                return p == std::u32string::npos || p >= static_cast<size_t>(selectionEnd);
            }
        }

        if (textLen == static_cast<size_t>(cursor)) {
            realtextLen.store(textLen, std::memory_order_release);
            return false;
        }

        // Heuristic: rapid text growth in a single-line context.
        // Applied only when no newline is present after the cursor to distinguish from AI text in editors.
        // Gecko/Firefox: if buffLen > textLen, surrounding text is stale (async update race)
        if (buffLen > textLen) {
            return false;
        }
        if (textLen > static_cast<size_t>(cursor) + 1 && cursor == realtextLen.load(std::memory_order_acquire) && u32Text.find(U'\n', cursor) == std::u32string::npos && sameprefix)
            return true;

        for (auto v = realtextLen.load(std::memory_order_acquire); v < cursor && !realtextLen.compare_exchange_weak(v, cursor, std::memory_order_acq_rel);)
            ;
        return false;
    }

    void LotusState::handlePreeditMode(KeyEvent& keyEvent, KeySym currentSym) {
        if (EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U)
            keyEvent.filterAndAccept();
        if (auto commit = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()))) {
            if (commit && (*commit.get() != 0)) {
                NGOSEN_INFO("Commit: " + std::string(commit.get()));
                host_->commitText(commit.get());
            }
        }
        host_->resetPanel();
        UniqueCPtr<char> preedit(EnginePullPreedit(lotusEngine_.handle()));
        if (preedit && (*preedit.get() != 0)) {
            std::string_view view = preedit.get();
            host_->showPreedit(ngosen::utf8::validate(view) ? std::string(view) : std::string(), false);
        }
        host_->refreshPreedit();
        host_->refreshPanel();
    }

    void LotusState::updateEmojiPageStatus() {
        const auto list = host_->candidates();
        if (!list || list->total == 0) {
            return;
        }

        int pageSize = list->pageSize;
        if (pageSize <= 0) {
            pageSize = 9;
        }

        int         totalItems  = list->total;
        int         currentPage = list->page + 1;
        int         totalPages  = (totalItems + pageSize - 1) / pageSize;

        std::string status = _("Page ") + std::to_string(currentPage) + "/" + std::to_string(totalPages);
        host_->setStatus(status);
    }

    void LotusState::pickEmoji(const EmojiEntry& entry) {
        host_->commitText(entry.output);
        NGOSEN_INFO("Emoji committed: " + entry.output);

        engine_->emojiLoader().recordHistory(entry);

        emojiBuffer_.clear();
        emojiCandidates_.clear();

        host_->resetPanel();
        host_->refreshPanel();
        updateEmojiPreedit();
    }

    void LotusState::handleEmojiMode(KeyEvent& keyEvent) {
        const KeySym currentSym      = keyEvent.rawKey().sym();
        bool         isCtrlBackspace = isBackspace(currentSym) && ((keyEvent.rawKey().states() & KeyState::Ctrl) != 0U);

        if (keyEvent.key().hasModifier() && !isCtrlBackspace) {
            keyEvent.forward();
            return;
        }

        const auto list = host_->candidates();
        if (list && currentSym >= FcitxKey_1 && currentSym <= FcitxKey_9) {
            int offset      = currentSym - FcitxKey_1;
            int globalIndex = (list->page * list->pageSize) + offset;

            if (globalIndex < list->total) {
                host_->pickCandidate(globalIndex);
                keyEvent.filterAndAccept();
                return;
            }
        }

        if (list && list->total > 0) {
            int  globalCursorIndex = list->cursor;
            int  totalSize         = list->total;
            int  currentPage       = list->page;
            int  pageSize          = list->pageSize;
            int  localCursorIndex  = globalCursorIndex - (currentPage * pageSize);

            bool handled = false;

            switch (currentSym) {
                case FcitxKey_Tab:
                case FcitxKey_Down: {
                    if (localCursorIndex < pageSize - 1 && globalCursorIndex < totalSize - 1) {
                        host_->highlightCandidate(globalCursorIndex + 1);
                    } else {
                        host_->highlightCandidate(currentPage * pageSize);
                    }
                    handled = true;
                    break;
                }

                case FcitxKey_ISO_Left_Tab:
                case FcitxKey_Up: {
                    if (localCursorIndex > 0) {
                        host_->highlightCandidate(globalCursorIndex - 1);
                    } else {
                        int lastIndex = std::min((currentPage * pageSize) + pageSize - 1, totalSize - 1);
                        host_->highlightCandidate(lastIndex);
                    }
                    handled = true;
                    break;
                }
                case FcitxKey_Page_Down:
                case FcitxKey_Right: {
                    if (list->hasNext) {
                        host_->nextCandidatePage();
                        int newPage = host_->candidates()->page;
                        host_->highlightCandidate(newPage * pageSize);
                        handled = true;
                    }
                    break;
                }
                case FcitxKey_Page_Up:
                case FcitxKey_Left: {
                    if (list->hasPrev) {
                        host_->prevCandidatePage();
                        int newPage = host_->candidates()->page;
                        host_->highlightCandidate(newPage * pageSize);
                        handled = true;
                    }
                    break;
                }
                default: break;
            }

            if (handled) {
                updateEmojiPageStatus();
                host_->refreshPanel();
                keyEvent.filterAndAccept();
                return;
            }
        }

        if (isBackspace(currentSym)) {
            if (!emojiBuffer_.empty()) {
                if (isCtrlBackspace) {
                    emojiBuffer_.clear();
                } else {
                    eraseLastUtf8Codepoint(emojiBuffer_);
                }
                keyEvent.filterAndAccept();
            } else {
                keyEvent.forward();
            }
            updateEmojiPreedit();
            return;
        }

        switch (currentSym) {
            case FcitxKey_space:
            case FcitxKey_Return: {
                if (list && list->total > 0) {
                    host_->pickCandidate(list->cursor);
                    keyEvent.filterAndAccept();
                } else if (currentSym == FcitxKey_Return && !emojiBuffer_.empty()) {
                    host_->commitText(emojiBuffer_);
                    emojiBuffer_.clear();
                    updateEmojiPreedit();
                    keyEvent.filterAndAccept();
                } else {
                    keyEvent.forward();
                }
                return;
            }

            case FcitxKey_Escape: {
                emojiBuffer_.clear();
                emojiCandidates_.clear();
                host_->resetPanel();
                host_->refreshPanel();
                keyEvent.filterAndAccept();
                return;
            }

            default: break;
        }

        {
            std::string utf8Char = Key::keySymToUTF8(currentSym);
            if (!utf8Char.empty()) {
                emojiBuffer_.append(utf8Char);
                keyEvent.filterAndAccept();
                updateEmojiPreedit();
            } else {
                keyEvent.forward();
            }
        }
    }
    void LotusState::updateEmojiPreedit() {
        if (emojiBuffer_.empty()) {
            emojiCandidates_ = engine_->emojiLoader().history();
            if (emojiCandidates_.empty()) {
                host_->resetPanel();
                host_->refreshPreedit();
                host_->refreshPanel();
                return;
            }
        } else {
            emojiCandidates_ = engine_->emojiLoader().search(emojiBuffer_);
        }

        if (!emojiBuffer_.empty()) {
            host_->showPreedit(emojiBuffer_, true);
        } else {
            host_->clearPreedit();
        }

        if (!emojiCandidates_.empty()) {
            std::vector<std::string> labels;
            labels.reserve(emojiCandidates_.size());
            for (size_t i = 0; i < emojiCandidates_.size(); ++i) {
                size_t localIndex = (i % 9) + 1;
                if (emojiBuffer_.empty()) {
                    labels.push_back(std::to_string(localIndex) + ": " + emojiCandidates_[i].output);
                } else {
                    labels.push_back(std::to_string(localIndex) + ": " + emojiCandidates_[i].trigger + " " + emojiCandidates_[i].output);
                }
            }
            // The list keeps its own copy: emojiCandidates_ may change while it is still shown.
            host_->showCandidates(labels, 9, [this, entries = emojiCandidates_](size_t index) { pickEmoji(entries[index]); });
            updateEmojiPageStatus();
        } else {
            host_->hideCandidates();
        }

        host_->refreshPreedit();
        host_->refreshPanel();
    }

    bool LotusState::deletionLooksDone() const {
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            return false;
        }
        const std::string& t = s.text();
        // A snapshot identical to the one taken when the backspaces were sent cannot mean "done" by
        // content alone: when the app lags by a couple of keys, the stale snapshot looks exactly like
        // the finished state.
        if (!surr_wait_sent_snapshot_.empty() && t + "\x1f" + std::to_string(s.cursor()) == surr_wait_sent_snapshot_) {
            // Firefox may never send another state, so accept it after WaitSurroundingMinPerKeyMs per
            // backspace, as long as a plain sleep. The immediate check still rejects it.
            const auto waited  = (ngosen::monotonicUs() - surr_wait_started_at_) / 1000;
            const auto minimum = static_cast<uint64_t>(engine_->options().waitSurroundingMinPerKeyMs) * static_cast<uint64_t>(std::max(expected_backspaces_, 1));
            if (waited < minimum) {
                return false;
            }
        }
        auto it = t.begin();
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
            it = ngosen::utf8::nextChar(it, t.end());
        }
        const std::string before(t.begin(), it);
        auto              endsWith = [](const std::string& a, const std::string& b) { return a.size() >= b.size() && a.compare(a.size() - b.size(), b.size(), b) == 0; };
        if (!surr_wait_deleted_.empty() && endsWith(before, surr_wait_prefix_ + surr_wait_deleted_)) {
            return false; // stale snapshot: the text to delete is still there
        }
        // Messenger moves the cursor before it removes the text, so a snapshot can look done while
        // deleted chars still follow the cursor. Treat that as in progress (#267).
        if (!surr_wait_deleted_.empty() && it != t.end()) {
            const std::string charAfter(it, ngosen::utf8::nextChar(it, t.end()));
            std::string       passed = surr_wait_prefix_;
            for (auto d = surr_wait_deleted_.begin(); d != surr_wait_deleted_.end();) {
                const auto next = ngosen::utf8::nextChar(d, surr_wait_deleted_.end());
                if (charAfter == std::string(d, next) && endsWith(before, passed)) {
                    return false;
                }
                passed.append(d, next);
                d = next;
            }
        }
        return endsWith(before, surr_wait_prefix_);
    }

    // The wait returns to the event loop, so focus can move while a commit is pending. deactivate()
    // clears is_deleting_ and a later timer would drop the text, so commit now.
    void LotusState::flushPendingReplacement() {
        if (!surr_wait_pending_) {
            return;
        }
        // Committing before a timer-only wait ends would land before the backspaces. The focus loss is
        // usually Chromium X11 bouncing focus, so let the timer commit.
        if (surr_wait_timer_only_) {
            return;
        }
        finishReplacement("focus lost", false);
    }

    namespace {
        // The Messenger composer has "\n\n" right after the cursor, or is just "\n" when empty. Only
        // this field repaints over fresh text, so only it waits to settle.
        bool looksLikeMessengerComposer(const ngosen::Surrounding& s) {
            if (!s.isValid()) {
                return false;
            }
            const std::string& t = s.text();
            if (t == "\n") {
                return true;
            }
            auto it = t.begin();
            for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
                it = ngosen::utf8::nextChar(it, t.end());
            }
            return std::string(it, t.end()) == "\n\n";
        }

        // Facebook composers (message and post box) report text ending in "\n\n" wherever the cursor
        // is, or just "\n" when empty. Check the whole field so mid-text edits match too.
        bool looksLikeFacebookComposer(const ngosen::Surrounding& s) {
            if (!s.isValid()) {
                return false;
            }
            const std::string& t = s.text();
            return t == "\n" || (t.size() >= 2 && t.compare(t.size() - 2, 2, "\n\n") == 0);
        }

        // No space or newline before the cursor: the first word of the message is being typed.
        bool isFirstWordOfMessage(const ngosen::Surrounding& s) {
            const std::string& t   = s.text();
            auto               end = t.begin();
            for (unsigned int i = 0; i < s.cursor() && end != t.end(); ++i) {
                end = ngosen::utf8::nextChar(end, t.end());
            }
            return std::find_if(t.begin(), end, [](char c) { return c == ' ' || c == '\n'; }) == end;
        }
    } // namespace

    void LotusState::deliverAfterSettle(const char* reason, bool fromTimer) {
        const auto snapshot = host_->surrounding();
        int        settleMs = engine_->options().waitSurroundingSettleMs;
        if (settleMs <= 0 || !looksLikeMessengerComposer(snapshot)) {
            finishReplacement(reason, fromTimer);
            return;
        }
        // A just-emptied composer is still reloading its placeholder, so the first word waits longer.
        if (isFirstWordOfMessage(snapshot)) {
            settleMs = std::max(settleMs, engine_->options().waitSurroundingSettleFirstWordMs);
        }
        // Reuse the timer-only wait state: a key arriving mid-wait finishes the wait first, focus loss
        // leaves the commit to the timer, and the snapshot watcher and timeout stay quiet.
        surr_wait_pending_       = true; // the immediate path gets here without the flag set
        surr_wait_timer_only_    = true;
        surr_wait_focus_retries_ = 0;
        settle_reason_           = reason;
        surr_wait_deliver_at_    = ngosen::monotonicUs() + (static_cast<uint64_t>(settleMs) * 1000ULL);
        settle_timer_            = host_->startTimer(surr_wait_deliver_at_, 1000, [this](ngosen::Timer&) {
            if (!surr_wait_pending_ || !surr_wait_timer_only_ || !is_deleting_.load()) {
                return false;
            }
            finishReplacement(settle_reason_, false);
            return false;
        });
    }

    void LotusState::finishReplacement(const char* reason, bool fromTimer) {
        const auto elapsedMs = (ngosen::monotonicUs() - surr_wait_started_at_) / 1000;
        NGOSEN_INFO("Surr wait " + std::string(reason) + " after " + std::to_string(elapsedMs) + " ms");
        if (std::string(reason) == "timeout") {
            surr_snapshot_trusted_ = false;
            ++surr_timeout_streak_;
            // Frozen means at least two events, all identical to the send-time snapshot. A Firefox that
            // is merely lagging usually sends nothing or a snapshot that is still moving.
            if (surr_wait_event_count_ >= 2 && !surr_wait_saw_other_snapshot_ && surr_wait_sent_snapshot_fresh_) {
                if (++surr_frozen_streak_ >= 2 && !surr_frozen_) {
                    surr_frozen_             = true;
                    surr_frozen_probe_count_ = 0;
                    NGOSEN_INFO("Surr frozen: stop waiting");
                }
            } else {
                surr_frozen_streak_ = 0;
            }
        } else if (std::string(reason) == "event" || std::string(reason) == "threshold") {
            surr_snapshot_trusted_ = true;
            surr_timeout_streak_   = 0;
            surr_frozen_streak_    = 0;
            if (surr_frozen_) {
                surr_frozen_ = false;
                NGOSEN_INFO("Surr frozen: resume waiting");
            }
        }
        surr_wait_pending_    = false;
        surr_wait_timer_only_ = false;
        if (!fromTimer && surr_wait_timer_) {
            surr_wait_timer_.reset(); // never reset a timer from inside its own callback
        }
        // A previous replacement still waiting for its turn goes out first, keeping the text in order.
        flushDeferredCommit();
        // Focus loss is not inside a key event, and the field is going away, so it commits at once.
        const bool defer = host_->field().frontend == "dbus" && std::string(reason) != "focus lost";
        if (defer) {
            deferred_commit_text_    = pending_commit_string_;
            deferred_commit_pending_ = true;
            deferred_commit_timer_   = host_->startTimer(ngosen::monotonicUs(), 0, [this](ngosen::Timer&) {
                flushDeferredCommit();
                return false; // never reset a timer from inside its own callback
            });
        }
        std::string text         = defer ? std::string() : std::move(pending_commit_string_);
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        pending_commit_string_.clear();
        is_deleting_.store(false);
        if (!defer) {
            replayBufferedKeys(std::move(text));
        }
    }

    void LotusState::flushDeferredCommit() {
        if (!deferred_commit_pending_) {
            return;
        }
        deferred_commit_pending_ = false;
        std::string text         = std::move(deferred_commit_text_);
        deferred_commit_text_.clear();
        replayBufferedKeys(std::move(text));
    }

    bool LotusState::handleUInputKeyPress(KeyEvent& event, KeySym currentSym, int sleepTime) {
        if (!is_deleting_.load()) {
            return false;
        }
        if (isBackspace(currentSym)) {
            current_backspace_count_ += 1;
            if (current_backspace_count_ < expected_backspaces_) {
                return false; // Allow intermediate backspaces to reach the app to clear autofill/old text.
            }
            return waitForDeletion(&event, sleepTime);
        }
        return false;
    }

    // Waits until the app has applied the backspaces, then commits. `event` is the returning sentinel
    // backspace on the XTEST path, or null when the backspaces were forwarded.
    bool LotusState::waitForDeletion(KeyEvent* event, int sleepTime) {
        // Some apps (Konsole) declare surrounding text but always send it empty; nothing can match,
        // so use the sleeping path.
        const bool emptySnapshot = host_->surrounding().text().empty();
        // GNOME Shell relays surrounding text late and one step behind (or not at all), so an
        // event-driven wait times out and commits ahead of the backspaces. Use the timed path there.
        const bool waitEvent = engine_->options().waitSurroundingEvent && !ngosen::surroundingTextLags(host_->field());
        if (waitEvent && emptySnapshot) {
            NGOSEN_INFO("Surr wait skip: empty snapshot");
        }
        bool skipFrozenWait = false;
        if (waitEvent && !emptySnapshot && surr_frozen_) {
            const int probeEvery = std::max(engine_->options().waitSurroundingProbeEvery, 1);
            ++surr_frozen_probe_count_;
            if (surr_frozen_probe_count_ % probeEvery != 0) {
                skipFrozenWait = true;
            }
        }
        if (waitEvent && !emptySnapshot && !skipFrozenWait) {
            // Sleeping blocks the single event loop, so no update could arrive. Return to the loop
            // and watch for updates from now on; a fresh watcher ignores the previous replacement's
            // late events. Keys typed meanwhile go to buffered_keys_.
            if (event != nullptr) {
                event->filterAndAccept();
            }
            surr_wait_started_at_ = ngosen::monotonicUs();
            // After a timeout the app is lagging (Firefox) and its snapshot is stale: skip the
            // immediate check.
            if (surr_snapshot_trusted_ && deletionLooksDone()) {
                NGOSEN_INFO("Skip retry");
                deliverAfterSettle("immediate", false);
                return true;
            }
            surr_wait_pending_            = true;
            surr_wait_event_count_        = 0;
            surr_wait_saw_other_snapshot_ = false;
            surr_wait_watching_           = true;
            // Two timeouts in a row: this app does not update while deleting. Use the short timeout
            // until an event matches again.
            const int  timeoutMs = surr_timeout_streak_ >= 2 ? engine_->options().waitSurroundingShortMs : engine_->options().waitSurroundingTimeoutMs;
            const auto timeout   = static_cast<uint64_t>(timeoutMs) * 1000ULL;
            // Accuracy 0 means sd-event's default 250 ms slack, so pass 1 ms. Check once at the
            // threshold first: many apps report "done" before it and then go quiet.
            const auto threshold     = static_cast<uint64_t>(engine_->options().waitSurroundingMinPerKeyMs) * static_cast<uint64_t>(std::max(expected_backspaces_, 1)) * 1000ULL;
            const auto firstDeadline = threshold < timeout ? surr_wait_started_at_ + threshold : surr_wait_started_at_ + timeout;
            surr_wait_timer_         = host_->startTimer(firstDeadline, 1000, [this, timeout](ngosen::Timer& t) {
                if (!surr_wait_pending_ || surr_wait_timer_only_ || !is_deleting_.load()) {
                    return false;
                }
                const auto waited = ngosen::monotonicUs() - surr_wait_started_at_;
                if (waited + 1000 < timeout) {
                    if (deletionLooksDone()) {
                        deliverAfterSettle("threshold", true);
                        return false;
                    }
                    t.rearm(surr_wait_started_at_ + timeout);
                    return true;
                }
                finishReplacement("timeout", true);
                return false;
            });
            return true;
        }
        // Frozen snapshot: sleep at least WaitSurroundingMinPerKeyMs x (N - 1) instead of, not on
        // top of, the normal sleep.
        const int perKeyMs = skipFrozenWait ? std::max(sleepTime, engine_->options().waitSurroundingMinPerKeyMs) : sleepTime;
        int       waitMs   = perKeyMs * (expected_backspaces_ - 1);
        // Validate surr cursor pos should match realtextLen after all BS applied
        const auto surr = host_->surrounding();
        if (skipFrozenWait) {
            NGOSEN_INFO("Skip retry (frozen)"); // retrying 3 x 2 ms is pointless on a frozen snapshot
        } else if (surr.isValid() && surr.cursor() == realtextLen.load(std::memory_order_acquire)) {
            NGOSEN_INFO("Skip retry");
        } else if (!host_->field().surroundingText) {
            // No surrounding text capability (gnome-terminal, Chromium on X11): retrying cannot help.
            NGOSEN_INFO("Skip retry (no surrounding capability)");
        } else {
            // Retry x3 (2 ms each) for apps whose snapshot is not valid yet. Use a timer, not
            // sleep_for, so the event loop can deliver a fresh snapshot.
            waitMs += 3 * 2;
        }
        if (event != nullptr) {
            event->filterAndAccept(); // filter out the returning sentinel backspace
        }
        if (waitMs <= 0) {
            finishReplacement("immediate", false);
            return true;
        }
        // Wait on a timer rather than sleep_for, which would block the fcitx5 event loop. Keys
        // arriving meanwhile go to buffered_keys_ because is_deleting_ is set.
        surr_wait_pending_       = true;
        surr_wait_timer_only_    = true;
        surr_wait_focus_retries_ = 0;
        surr_wait_started_at_    = ngosen::monotonicUs();
        surr_wait_deliver_at_    = surr_wait_started_at_ + (static_cast<uint64_t>(waitMs) * 1000ULL);
        surr_wait_timer_         = host_->startTimer(surr_wait_deliver_at_, 1000, [this](ngosen::Timer& t) {
            if (!surr_wait_pending_ || !surr_wait_timer_only_) {
                return false;
            }
            if (!is_deleting_.load()) { // the replacement was cancelled elsewhere (navigation key...)
                surr_wait_pending_    = false;
                surr_wait_timer_only_ = false;
                return false;
            }
            if (!host_->hasFocus()) {
                // Chromium X11 leaves and re-enters the field within ~0.3 ms; a commit in that gap is
                // lost. Give it a moment to come back.
                if (++surr_wait_focus_retries_ <= 5) {
                    t.rearm(ngosen::monotonicUs() + 2000);
                    return true;
                }
                // The user really switched windows: the old field can no longer take the text. Reset
                // only this field's state; is_deleting_ is shared and the new field may be replacing.
                NGOSEN_INFO("Timer: input context lost focus, dropping text");
                surr_wait_pending_       = false;
                surr_wait_timer_only_    = false;
                expected_backspaces_     = 0;
                current_backspace_count_ = 0;
                pending_commit_string_.clear();
                return false;
            }
            finishReplacement("timer", true);
            return false;
        });
        return true;
    }

    void LotusState::forwardBackspaces(int count) {
        for (int i = 0; i < count; ++i) {
            host_->forwardKey(ngosen::EditKey::BackSpace, false);
            host_->forwardKey(ngosen::EditKey::BackSpace, true);
        }
    }

    bool LotusState::canSendBackspaces() const {
        return ngosen::forwardsBackspaces(host_->field()) || xtestAvailable();
    }

    void LotusState::deferTimedCommit(uint64_t deliverAtUs) {
        if (surr_wait_timer_ && deliverAtUs > surr_wait_deliver_at_) {
            surr_wait_deliver_at_ = deliverAtUs;
            surr_wait_timer_->rearm(deliverAtUs);
        }
    }

    void LotusState::surroundingUpdated() {
        // A wait that starts while this report is handled sees only the next report.
        const bool waiting    = surr_wait_watching_;
        const bool overtyping = overtype_watching_;
        if (host_->hasFocus())
            checkCursorJump();
        if (waiting)
            onWaitSurroundingUpdated();
        if (overtyping)
            onOvertypeSurroundingUpdated();
    }

    void LotusState::onWaitSurroundingUpdated() {
        if (!surr_wait_pending_ || surr_wait_timer_only_ || !is_deleting_.load()) {
            return;
        }
        // After a timeout, an early event is the app's stale buffer catching up, not the
        // finished deletion. Ignore events before WaitSurroundingMinPerKeyMs per backspace.
        const auto waitedUs  = ngosen::monotonicUs() - surr_wait_started_at_;
        const auto minimumUs = static_cast<uint64_t>(engine_->options().waitSurroundingMinPerKeyMs) * static_cast<uint64_t>(std::max(expected_backspaces_, 1)) * 1000ULL;
        {
            const auto current = host_->surrounding();
            ++surr_wait_event_count_;
            if (current.text() + "\x1f" + std::to_string(current.cursor()) != surr_wait_sent_snapshot_) {
                surr_wait_saw_other_snapshot_ = true;
            }
        }
        if (!surr_snapshot_trusted_ && waitedUs < minimumUs) {
            // Stale buffer catching up after a timeout: ignore.
        } else if (deletionLooksDone()) {
            deliverAfterSettle("event", false);
        }
        // Not done yet: keep waiting silently. Anything worth printing here is text the user
        // just typed, which must not go into the log.
    }

    void LotusState::onOvertypeSurroundingUpdated() {
        if (!overtype_pending_ || !is_deleting_.load()) {
            return;
        }
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            return;
        }
        const int selected = static_cast<int>(s.anchor()) - static_cast<int>(s.cursor());
        if (selected != overtype_char_count_ && selected != -overtype_char_count_) {
            return;
        }
        finishOvertype("selected", false);
    }

    // Wayland apps report a click only as a cursor move; the IM gets no reset or mouse event.
    void LotusState::checkCursorJump() {
        const auto s = host_->surrounding();
        if (!s.isValid()) {
            hasLastSurrounding_ = false;
            return;
        }
        const bool sameText = hasLastSurrounding_ && s.text() == lastSurroundingText_;
        bool       jumped   = sameText && (s.cursor() != lastSurroundingCursor_ || s.anchor() != lastSurroundingAnchor_);
        // Some editors (Lark in Firefox) report the cursor past our commit before the committed text.
        const bool echo = jumped && unreportedCommitLength_ > 0 && s.cursor() == s.anchor() && s.cursor() == lastSurroundingCursor_ + unreportedCommitLength_;
        // Firefox repeats the old state before the echo, so only a change ends the wait.
        if (jumped || !sameText)
            unreportedCommitLength_ = 0;
        jumped                 = jumped && !echo;
        lastSurroundingText_   = s.text();
        lastSurroundingCursor_ = s.cursor();
        lastSurroundingAnchor_ = s.anchor();
        hasLastSurrounding_    = true;
        // Our own selection for overtyping moves the anchor too.
        if (!jumped || is_deleting_.load(std::memory_order_acquire))
            return;
        NGOSEN_INFO("Cursor moved without an edit");
        needEngineReset.store(true, std::memory_order_release);
        g_mouse_clicked.store(true, std::memory_order_release);
    }

    void LotusState::noteCommit(const std::string& text) {
        unreportedCommitLength_ += ngosen::utf8::length(text);
    }

    void LotusState::sendSelectKeys(int charCount) const {
        sendBackspaceKeys(-charCount);
    }

    void LotusState::selectAndOvertype(const std::string& addedPart, int charCount, bool viaXTest) {
        is_deleting_.store(true, std::memory_order_release);
        overtype_via_xtest_      = viaXTest;
        overtype_shift_released_ = false;
        pending_commit_string_   = addedPart;
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        overtype_char_count_     = charCount;
        overtype_pending_        = true;
        {
            const auto snapshot     = host_->surrounding();
            overtype_had_snapshot_  = snapshot.isValid();
            overtype_cursor_before_ = overtype_had_snapshot_ ? snapshot.cursor() : 0;
        }
        overtype_started_at_     = ngosen::monotonicUs();
        overtype_watching_       = true;
        const uint64_t timeoutUs = viaXTest ? XTestSelectTimeoutUs : 150000ULL;
        overtype_timer_          = host_->startTimer(overtype_started_at_ + timeoutUs, 1000, [this](ngosen::Timer&) {
            if (overtype_pending_ && is_deleting_.load()) {
                if (overtype_shift_released_) {
                    finishOvertype("selection settled", true);
                } else {
                    abandonOvertype();
                }
            }
            return false;
        });
        // forwardKey does not carry Shift into the selection, so press the keys like a real keyboard.
        sendSelectKeys(charCount);
        NGOSEN_INFO("Select " + std::to_string(charCount) + " chars");
    }

    // The field did not report the selection in time. Do not type over it: the cursor has moved and
    // the text would land in the wrong place. Move the cursor back and drop this replacement; the
    // user loses one tone mark and sees it immediately.
    void LotusState::abandonOvertype() {
        const auto snapshot     = host_->surrounding();
        int        rightPresses = 1; // a real but unreported selection collapses with one Right
        if (overtype_had_snapshot_ && snapshot.isValid() && snapshot.cursor() == snapshot.anchor() &&
            snapshot.cursor() + static_cast<unsigned int>(overtype_char_count_) == overtype_cursor_before_) {
            rightPresses = overtype_char_count_; // the field only moved the cursor, nothing selected
        }
        for (int i = 0; i < rightPresses; ++i) {
            host_->forwardKey(ngosen::EditKey::Right, false);
            host_->forwardKey(ngosen::EditKey::Right, true);
        }
        NGOSEN_INFO("Overtype gave up after " + std::to_string((ngosen::monotonicUs() - overtype_started_at_) / 1000) + " ms, moved cursor right " + std::to_string(rightPresses));
        overtype_pending_   = false;
        overtype_via_xtest_ = false;
        pending_commit_string_.clear();
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        hasHistory_              = false;
        ResetEngine(lotusEngine_.handle());
        oldPreBuffer_.clear();
        is_deleting_.store(false);
        // Keys typed during the wait are user input; the cursor is back, so replay them.
        replayBufferedKeys();
    }

    void LotusState::finishOvertype(const char* reason, bool fromTimer) {
        const auto elapsedMs = (ngosen::monotonicUs() - overtype_started_at_) / 1000;
        NGOSEN_INFO("Overtype " + std::string(reason) + " after " + std::to_string(elapsedMs) + " ms");
        overtype_pending_   = false;
        overtype_via_xtest_ = false;
        if (!fromTimer && overtype_timer_) {
            overtype_timer_.reset(); // never reset a timer from inside its own callback
        }
        std::string text = std::move(pending_commit_string_);
        pending_commit_string_.clear();
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        is_deleting_.store(false);
        replayBufferedKeys(std::move(text));
    }

    void LotusState::performReplacement(const std::string& deletedPart, const std::string& addedPart) {
        NGOSEN_INFO("Perform replacement: " + deletedPart + " -> " + addedPart); //NOLINT
        current_backspace_count_ = 0;
        pending_commit_string_   = addedPart;
        expected_backspaces_     = static_cast<int>(ngosen::utf8::length(deletedPart));
        surr_wait_deleted_       = deletedPart;
        {
            const auto snapshot      = host_->surrounding();
            surr_wait_sent_snapshot_ = snapshot.isValid() ? snapshot.text() + "\x1f" + std::to_string(snapshot.cursor()) : std::string();
        }
        surr_wait_prefix_ = (oldPreBuffer_.size() >= deletedPart.size()) ? oldPreBuffer_.substr(0, oldPreBuffer_.size() - deletedPart.size()) : std::string();
        {
            // Only a fresh send-time snapshot (text before the cursor ends with prefix + deleted) may
            // count towards "frozen"; a lagging app sends a stale one.
            surr_wait_sent_snapshot_fresh_ = false;
            const auto snapshot            = host_->surrounding();
            if (snapshot.isValid()) {
                const std::string& t  = snapshot.text();
                auto               it = t.begin();
                for (unsigned int i = 0; i < snapshot.cursor() && it != t.end(); ++i) {
                    it = ngosen::utf8::nextChar(it, t.end());
                }
                const std::string before(t.begin(), it);
                const std::string expected     = surr_wait_prefix_ + surr_wait_deleted_;
                surr_wait_sent_snapshot_fresh_ = before.size() >= expected.size() && before.compare(before.size() - expected.size(), expected.size(), expected) == 0;
            }
        }
        const auto        surrounding = host_->surrounding();
        const std::string surrText    = surrounding.text();
        // Facebook composers only: other fields do not report a selection-only change, so the
        // overtype would time out and drop the tone mark.
        // A BackSpace in the Chromium address bar would only remove the selected autocompletion. XTEST,
        // unlike forwardKey, keeps Shift, so widen the selection over the old text and type over both.
        if (xtestAvailable() && ngosen::selectsOverAutocompletion(host_->field()) && !deletedPart.empty()) {
            selectAndOvertype(addedPart, static_cast<int>(ngosen::utf8::length(deletedPart)), true);
            return;
        }
        if (engine_->options().messengerSelectOvertype && !ngosen::forwardsBackspaces(host_->field()) && looksLikeFacebookComposer(surrounding)) {
            selectAndOvertype(addedPart, static_cast<int>(ngosen::utf8::length(deletedPart)));
            return;
        }
        // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it. Its
        // deleteSurroundingText applies at once, relative to the cursor, so use it there (#162).
        const bool isLibreOffice   = ngosen::appliesBackspacesLate(host_->field());
        const bool mustUseSurrText = isLibreOffice || ngosen::ignoresForwardedKeys(host_->field());
        bool       isSurrText = mustUseSurrText ? host_->field().surroundingText :
                                                  engine_->options().useSurroundingTextIfPossible && host_->field().surroundingText && surrounding.isValid() && !surrText.empty() &&
                surrounding.cursor() == ngosen::utf8::length(surrText);
        if (!isSurrText) {
            ++expected_backspaces_;
            // Sen skips the autofill guard except in address bars (#190): the Url flag on Chromium,
            // the autofill shape on Firefox.
            const bool isFirefoxAddressBar = ngosen::hidesAddressBarFlag(host_->field()) && textAfterCursorLooksLikeUrl(surrounding);
            const bool checkAutofill       = realMode != LotusMode::Sen || host_->field().url || isFirefoxAddressBar;
            if (checkAutofill) {
                // Enable Autofill detection for all frontends (Wayland/IBus).
                // This fixes the "toôi" duplication bug in Chromium-based search bars.
                // The isAutofillCertain function has been optimized to differentiate
                // between browser autofill and AI ghost text.
                // isAutofillCertain runs first so its realtextLen update still happens.
                if (isAutofillCertain(surrounding) || (isFirefoxAddressBar && onlyCurrentWordBeforeCursor(surrounding, oldPreBuffer_))) {
                    ++expected_backspaces_;
                }
            }
        }
        is_deleting_.store(true, std::memory_order_release);
        if (isSurrText) {
            host_->deleteSurrounding(-expected_backspaces_, expected_backspaces_);
            NGOSEN_INFO("Delete using surrounding text");
            std::this_thread::sleep_for(std::chrono::milliseconds(engine_->options().surrDeleteSleepMs * expected_backspaces_));
            if (!pending_commit_string_.empty()) {
                host_->commitText(pending_commit_string_);
                NGOSEN_INFO("Commit: " + pending_commit_string_);
                std::this_thread::sleep_for(std::chrono::milliseconds(engine_->options().surrCommitSleepMs * ngosen::utf8::length(addedPart)));
            }
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            is_deleting_.store(false);
            replayBufferedKeys();
            return;
        }
        if (ngosen::forwardsBackspaces(host_->field())) {
            // The XTEST count includes a sentinel that comes back to us; forwarded keys never do.
            const int count = expected_backspaces_ - 1;
            if (host_->field().frontend == "xim") {
                // We are inside the client's synchronous XIM request for the key that triggered this
                // replacement. Keys forwarded now reach the client before its reply, and libX11 may
                // hand them back to us unprocessed. Forward them once the reply has gone out.
                xim_forward_timer_ = host_->startTimer(ngosen::monotonicUs(), 0, [this, count](ngosen::Timer&) {
                    if (is_deleting_.load()) {
                        forwardBackspaces(count);
                    }
                    return false;
                });
            } else {
                forwardBackspaces(count);
            }
            NGOSEN_INFO("Forward " + std::to_string(count) + " backspaces");
            waitForDeletion(nullptr, 4);
            // XIM, IBus and D-Bus clients queue forwarded keys, and the commit can overtake them.
            if (host_->field().frontend != "wayland" && surr_wait_timer_only_ && surr_wait_timer_) {
                deferTimedCommit(surr_wait_started_at_ + ForwardWaitUs);
            }
            return;
        }
        sendBackspaceKeys(expected_backspaces_);
        NGOSEN_INFO("Send " + std::to_string(expected_backspaces_) + " backspaces");
    }

    bool LotusState::checkForwardSpecialKey(KeyEvent& keyEvent, KeySym& currentSym) {
        if (keyEvent.key().isCursorMove() || currentSym == FcitxKey_Tab || currentSym == FcitxKey_KP_Tab || currentSym == FcitxKey_ISO_Left_Tab || currentSym == FcitxKey_Escape ||
            keyEvent.key().hasModifier()) {
            is_deleting_.store(false, std::memory_order_release);
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();
            return true;
        }

        if (currentSym == FcitxKey_Delete) {
            return true;
        }

        if (currentSym >= FcitxKey_KP_0 && currentSym <= FcitxKey_KP_9) {
            currentSym = static_cast<KeySym>(FcitxKey_0 + (currentSym - FcitxKey_KP_0));
            return false;
        }

        switch (currentSym) {
            case FcitxKey_KP_Add: {
                currentSym = FcitxKey_plus;
                break;
            }
            case FcitxKey_KP_Subtract: {
                currentSym = FcitxKey_minus;
                break;
            }
            case FcitxKey_KP_Divide: {
                currentSym = FcitxKey_slash;
                break;
            }
            case FcitxKey_KP_Multiply: {
                currentSym = FcitxKey_asterisk;
                break;
            }
            case FcitxKey_KP_Decimal: {
                currentSym = FcitxKey_period;
                break;
            }
            case FcitxKey_KP_Enter: {
                currentSym = FcitxKey_Return;
                break;
            }
            case FcitxKey_KP_Equal: {
                currentSym = FcitxKey_equal;
                break;
            }
            case FcitxKey_KP_Space: {
                currentSym = FcitxKey_space;
                break;
            }
            default: break;
        }
        return false;
    }

    void LotusState::handleUinputMode(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }

        if (isBackspace(currentSym) || currentSym == FcitxKey_Return) {
            if (isBackspace(currentSym)) {
                hasHistory_ = true;
                EngineProcessKeyEvent(lotusEngine_.handle(), FcitxKey_BackSpace, 0);
                UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
                oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            } else {
                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.forward();
            return;
        }

        std::string keyUtf8 = Key::keySymToUTF8(currentSym);
        if (keyUtf8.empty()) {
            keyEvent.forward();
            return;
        }

        bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

        auto commitF = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
        if (commitF && (*commitF.get() != 0)) {
            std::string commitStr = commitF.get();
            std::string deletedPart;
            std::string addedPart;
            compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

            if (!deletedPart.empty()) {
                performReplacement(deletedPart, addedPart);
                keyEvent.filterAndAccept();
            } else {
                bool wasAutoCapitalized = (currentSym != keyEvent.rawKey().sym());
                if (!addedPart.empty() && (keyUtf8 != addedPart || wasAutoCapitalized)) {
                    // Prevent auto-capitalized character replacement from stripping out Vietnamese chars
                    if (addedPart.size() > 1 && addedPart.back() == ' ') {
                        // Stripping the trigger key (space) from addedPart
#if __cplusplus >= 202002L
                        addedPart.resize(addedPart.size() - 1);
#else
                        addedPart = addedPart.substr(0, addedPart.size() - 1);
#endif
                    }
                    host_->commitText(addedPart);
                    NGOSEN_INFO("Commit: " + addedPart);
                    keyEvent.filterAndAccept();
                } else {
                    keyEvent.forward();
                }
            }

            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();

            return;
        }

        if (!processed) {
            UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
            if (!preeditC || (*preeditC.get() == 0)) {
                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
                keyEvent.forward();
            }
            return;
        }

        hasHistory_ = true;
        realtextLen.fetch_add(1, std::memory_order_acq_rel);

        UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
        std::string      preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

        std::string      deletedPart;
        std::string      addedPart;

        if (compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
            if (deletedPart.empty()) {
                bool isCommit           = false;
                bool wasAutoCapitalized = (currentSym != keyEvent.rawKey().sym());
                if (!addedPart.empty()) {
                    oldPreBuffer_ = preeditStr;
                    if (wasAutoCapitalized || addedPart != keyUtf8) {
                        host_->commitText(addedPart);
                        NGOSEN_INFO("Commit: " + addedPart);
                        keyEvent.filterAndAccept();
                        isCommit = true;
                    }
                }
                if (!isCommit) {
                    keyEvent.forward();
                }
            } else {
                if (!canSendBackspaces()) {
                    NGOSEN_ERROR("Cannot send backspaces here, commit rawkey");
                    std::string rawKey = keyEvent.key().toString();
                    if (!rawKey.empty()) {
                        host_->commitText(rawKey);
                    }
                    return;
                }

                if (is_deleting_.load()) {
                    is_deleting_.store(false, std::memory_order_release);
                }

                keyEvent.filterAndAccept();
                performReplacement(deletedPart, addedPart);
                oldPreBuffer_ = preeditStr;
            }
        }
    }

    void LotusState::handleDoubleSpaceReplacement() {
        performReplacement(" ", ". ");
        NGOSEN_INFO("Commit: . ");
        if (engine_->options().autoCapitalizeAfterPunctuation) {
            isPrevPunctuation_ = true;
            shouldCapitalize_  = true;
        }
    }

    void LotusState::handleDoubleHyphenReplacement() {
        // Em-dash (U+2014)
        std::string emDash = "—";
        performReplacement("-", emDash);
        NGOSEN_INFO("Commit: — (em-dash)");
    }

    void LotusState::handleOffModeMacro(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }

        if (isBackspace(currentSym)) {
            EngineProcessKeyEvent(lotusEngine_.handle(), FcitxKey_BackSpace, 0);
            auto preeditC = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));
            oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            keyEvent.forward();
            return;
        }

        if (currentSym == FcitxKey_Return) {
            if (!oldPreBuffer_.empty()) {
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.forward();
            return;
        }

        std::string keyUtf8 = Key::keySymToUTF8(currentSym);

        bool        processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

        auto        commitPtr = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
        if (processed && commitPtr && (*commitPtr.get() != 0)) {
            std::string commitStr = commitPtr.get();

            // Determine if this is a macro expansion or just confirmed typed text
            bool isMacroExpansion = false;
            if (keyUtf8.empty()) {
                isMacroExpansion = (commitStr != oldPreBuffer_);
            } else {
                isMacroExpansion = (commitStr != oldPreBuffer_ + keyUtf8);
            }

            if (isMacroExpansion) {
                NGOSEN_INFO("Macro expansion: '" + oldPreBuffer_ + "' -> '" + commitStr + "'");
                // Try backspaces first, fallback to deleteSurroundingText, then plain commit
                if (canSendBackspaces() && !oldPreBuffer_.empty()) {
                    performReplacement(oldPreBuffer_, commitStr);
                } else if (host_->field().surroundingText) {
                    const auto surrounding = host_->surrounding();
                    if (surrounding.isValid()) {
                        size_t oldLen = ngosen::utf8::length(oldPreBuffer_);
                        if (oldLen > 0) {
                            host_->deleteSurrounding(-static_cast<int>(oldLen), static_cast<int>(oldLen));
                        }
                        host_->commitText(commitStr);
                    } else {
                        host_->commitText(commitStr);
                    }
                } else {
                    host_->commitText(commitStr);
                }
                keyEvent.filterAndAccept();
            } else {
                // No macro: typed text confirmed by engine, just forward trigger key
                keyEvent.forward();
            }

            oldPreBuffer_.clear();
            hasHistory_ = false;
            return;
        }

        // 8. No commit or engine rejected the key
        if (processed || (commitPtr && (*commitPtr.get() != 0))) {
            // Engine processed the key (building shadow state)
            // OR engine rejected the key but committed old text (non-processable key)
            auto preeditPtr = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));
            oldPreBuffer_   = (preeditPtr && (*preeditPtr.get() != 0)) ? preeditPtr.get() : "";
            if (!processed) {
                // Engine committed old text but didn't process the new key → forward the key
                oldPreBuffer_.clear();
                hasHistory_ = false;
            }
            keyEvent.forward();
        } else {
            // Engine didn't handle this key
            if (!oldPreBuffer_.empty()) {
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.forward();
        }
    }

    void LotusState::keyEvent(KeyEvent& keyEvent) {
        if (!lotusEngine_)
            return;
        if (overtype_pending_ && overtype_via_xtest_ && keyEvent.rawKey().sym() == FcitxKey_Shift_R) {
            keyEvent.forward();
            if (keyEvent.isRelease() && !overtype_shift_released_ && overtype_timer_) {
                overtype_shift_released_ = true;
                overtype_timer_->rearm(ngosen::monotonicUs() + XTestSelectSettleUs);
            }
            return;
        }
        if (realMode == LotusMode::Preedit) {
            if (keyEvent.rawKey().check(FcitxKey_Shift_L) || keyEvent.rawKey().check(FcitxKey_Shift_R))
                return;
        } else {
            if (keyEvent.rawKey().isModifier()) {
                handleModifierTap(keyEvent);
                return;
            }
            cancelModifierTap();
        }
        if (keyEvent.isRelease())
            return;
        if (const KeySym rawSym = keyEvent.rawKey().sym(); overtype_pending_ && (rawSym == FcitxKey_Left || rawSym == FcitxKey_Shift_L || rawSym == FcitxKey_Shift_R)) {
            // Our own Shift+Left selection: it must reach the app and must not be treated as the user
            // moving the cursor (that would discard the pending commit).
            keyEvent.forward();
            return;
        }
        // This safety valve silently clears the flag on the next key. Skip it while a wait is pending,
        // otherwise the pending commit is thrown away.
        if (!surr_wait_pending_ && !overtype_pending_ && current_backspace_count_ >= expected_backspaces_ && is_deleting_.load()) {
            is_deleting_.store(false);
            current_backspace_count_ = 0;
            expected_backspaces_     = 0;
            if (!buffered_keys_.empty()) {
                replayBufferedKeys();
            }
        }
        if (needEngineReset.load() && realMode != LotusMode::Off) {
            NGOSEN_INFO("Need engine reset");
            oldPreBuffer_.clear();
            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            is_deleting_.store(false);
            current_backspace_count_ = 0;
            isPrevSpace_             = false;
            shouldCapitalize_        = false;
            isPrevPunctuation_       = false;
            needEngineReset.store(false);
        }

        // A key that beats the deferred commit must not land before it.
        flushDeferredCommit();
        if (g_mouse_clicked.load(std::memory_order_acquire) && !is_deleting_.load(std::memory_order_acquire)) {
            g_mouse_clicked.store(false, std::memory_order_release);
            clearAllBuffers();
        }
        KeySym currentSym = keyEvent.rawKey().sym();
        if (engine_->options().autoCapitalizeAfterPunctuation && realMode != LotusMode::Off) {
            // Ignore auto-capitalize side-effects if we're processing automated replacement backspaces
            bool isAutomatedBackspace = is_deleting_.load(std::memory_order_acquire) && isBackspace(currentSym);

            if (!isAutomatedBackspace) {
                if (shouldCapitalize_) {
                    if (currentSym >= FcitxKey_a && currentSym <= FcitxKey_z) {
                        auto upperSym = static_cast<KeySym>(currentSym - (FcitxKey_a - FcitxKey_A));
                        currentSym    = upperSym;
                        keyEvent.setKey(Key(upperSym, keyEvent.rawKey().states()));
                        shouldCapitalize_ = false;
                    } else if (currentSym != FcitxKey_space) {
                        shouldCapitalize_ = false;
                    }
                }

                switch (currentSym) {
                    case FcitxKey_period:
                    case FcitxKey_exclam:
                    case FcitxKey_question: isPrevPunctuation_ = true; break;
                    case FcitxKey_Return:
                    case FcitxKey_KP_Enter:
                        shouldCapitalize_  = true;
                        isPrevPunctuation_ = false;
                        break;
                    case FcitxKey_space:
                        if (isPrevPunctuation_) {
                            shouldCapitalize_  = true;
                            isPrevPunctuation_ = false;
                        }
                        break;
                    default:
                        if (currentSym != FcitxKey_space) {
                            isPrevPunctuation_ = false;
                        }
                        break;
                }
            }
        }

        if (is_deleting_.load(std::memory_order_acquire) && surr_wait_timer_only_ && isBackspace(currentSym) && host_->field().frontend == "xim" &&
            ngosen::forwardsBackspaces(host_->field())) {
            // The XIM client handed a forwarded backspace back unprocessed. Let it through so the
            // client applies it, and commit after it.
            NGOSEN_INFO("XIM handed back a forwarded backspace");
            deferTimedCommit(ngosen::monotonicUs() + ForwardWaitUs);
            return;
        }
        if (is_deleting_.load(std::memory_order_acquire) && surr_wait_timer_only_) {
            // A key arrived during a timer-only wait. Replaying it via commitString loses text on
            // Chromium X11, so finish the wait, commit, then handle the key normally.
            const uint64_t nowUs = ngosen::monotonicUs();
            if (surr_wait_deliver_at_ > nowUs) {
                std::this_thread::sleep_for(std::chrono::microseconds(surr_wait_deliver_at_ - nowUs));
            }
            finishReplacement("key arrived", false);
        }
        if (is_deleting_.load(std::memory_order_acquire) && isBackspace(currentSym) && ngosen::forwardsBackspaces(host_->field()) && surr_wait_pending_) {
            // Forwarded backspaces never come back, so this one is the user's: finish the replacement
            // first, then handle it normally.
            finishReplacement("backspace arrived", false);
        }
        if (is_deleting_.load(std::memory_order_acquire)) {
            if (isBackspace(currentSym)) {
                if (realtextLen.load(std::memory_order_acquire) > 0)
                    realtextLen.fetch_sub(1, std::memory_order_acq_rel);
                if (handleUInputKeyPress(keyEvent, currentSym, 4)) {
                    return;
                }
            } else {
                std::string keyUtf8Check = Key::keySymToUTF8(currentSym);
                if (!keyUtf8Check.empty() && buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                    NGOSEN_WARN("Typing so fast, add key to queue");
                    buffered_keys_.push_back({.sym = currentSym, .state = keyEvent.rawKey().states()});
                }
                keyEvent.filterAndAccept();
            }
            return;
        }

        if (engine_->options().doubleSpaceToPeriod && realMode != LotusMode::Off) {
            bool isSpaceKey = (currentSym == FcitxKey_space || currentSym == FcitxKey_KP_Space);
            if (isSpaceKey && !keyEvent.key().hasModifier()) {
                if (isPrevSpace_) {
                    keyEvent.filterAndAccept();
                    handleDoubleSpaceReplacement();
                    isPrevSpace_ = false;
                    return;
                }
                isPrevSpace_ = true;
            } else {
                isPrevSpace_ = false;
            }
        }

        if (engine_->options().doubleHyphenToEmDash && realMode != LotusMode::Off) {
            bool isHyphenKey = (currentSym == FcitxKey_minus || currentSym == FcitxKey_KP_Subtract);
            if (isHyphenKey && !keyEvent.key().hasModifier()) {
                if (isPrevHyphen_) {
                    keyEvent.filterAndAccept();
                    handleDoubleHyphenReplacement();
                    isPrevHyphen_ = false;
                    return;
                }
                isPrevHyphen_ = true;
            } else {
                isPrevHyphen_ = false;
            }
        }

        switch (realMode) {
            case LotusMode::Sen: {
                handleUinputMode(keyEvent, currentSym);
                break;
            }
            case LotusMode::Preedit: {
                handlePreeditMode(keyEvent, currentSym);
                break;
            }
            case LotusMode::Emoji: {
                handleEmojiMode(keyEvent);
                break;
            }
            default: {
                if (engine_->options().enableMacroInOffMode && engine_->options().enableMacro) {
                    handleOffModeMacro(keyEvent, currentSym);
                }
                break;
            }
        }
        reEnableMacroAfterWordEnd();
    }

    void LotusState::reset(bool isFocusOut) {
        const auto  surrounding = host_->surrounding();
        const auto& text        = surrounding.text();
        size_t      textLen     = ngosen::utf8::length(text);
        realtextLen.store(textLen, std::memory_order_release);
        if (is_deleting_.load(std::memory_order_acquire)) {
            return;
        }
        resetMacroSkip();

        if (lotusEngine_) {
            isPrevSpace_       = false;
            isPrevHyphen_      = false;
            shouldCapitalize_  = false;
            isPrevPunctuation_ = false;
            if (realMode == LotusMode::Preedit && isFocusOut) {
                EngineCommitPreedit(lotusEngine_.handle());
                UniqueCPtr<char> commit(EnginePullCommit(lotusEngine_.handle()));
                if (commit && (*commit.get() != 0)) {
                    host_->commitText(commit.get());
                    NGOSEN_INFO("Commit: " + std::string(commit.get()));
                }
            }
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
        }
        if (host_->field().frontend != "dbus")
            clearAllBuffers();

        switch (realMode) {
            case LotusMode::Preedit: {
                host_->resetPanel();
                host_->refreshPanel();
                host_->refreshPreedit();
                break;
            }
            case LotusMode::Sen: {
                host_->resetPanel();
                break;
            }
            case LotusMode::Emoji: {
                host_->resetPanel();
                host_->refreshPanel();
                host_->refreshPreedit();
                break;
            }
            default: {
                break;
            }
        }
    }

    void LotusState::commitBuffer() {
        switch (realMode) {
            case LotusMode::Preedit: {
                host_->resetPanel();
                if (lotusEngine_) {
                    EngineCommitPreedit(lotusEngine_.handle());
                    UniqueCPtr<char> commit(EnginePullCommit(lotusEngine_.handle()));
                    if (commit && (*commit.get() != 0))
                        host_->commitText(commit.get());
                    ResetEngine(lotusEngine_.handle());
                }
                host_->refreshPanel();
                host_->refreshPreedit();
                break;
            }
            case LotusMode::Sen: {
                if (lotusEngine_) {
                    ResetEngine(lotusEngine_.handle());
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    void LotusState::clearAllBuffers() {
        NGOSEN_DEBUG("Clear all buffers");
        if (is_deleting_.load(std::memory_order_acquire)) {
            return;
        }
        resetMacroSkip();
        oldPreBuffer_.clear();
        hasHistory_ = false;
        if (!is_deleting_.load(std::memory_order_acquire)) {
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
        }
        emojiBuffer_.clear();
        emojiCandidates_.clear();
        buffered_keys_.clear();
        shouldCapitalize_  = false;
        isPrevSpace_       = false;
        isPrevHyphen_      = false;
        isPrevPunctuation_ = false;
        if (lotusEngine_)
            ResetEngine(lotusEngine_.handle());
    }

    bool LotusState::isEmptyHistory() const {
        return !hasHistory_;
    }

    void LotusState::replayBufferedKeys(std::string committed) {
        // Under GNOME, mutter sends one text-input "done" per main-loop turn and clients keep only the
        // last commit_string before it, so back-to-back commits lose all but the last ("đ" then "i"
        // shows "i"). Send the replacement and the replayed keys as one commit.
        std::string out   = std::move(committed);
        auto        flush = [&] {
            if (!out.empty()) {
                host_->commitText(out);
                NGOSEN_INFO("Commit: " + out);
                out.clear();
            }
        };
        NGOSEN_INFO("Starting replay buffered keys");
        if (buffered_keys_.empty()) {
            flush();
            return;
        }
        auto keys = std::move(buffered_keys_);
        buffered_keys_.clear();
        for (size_t i = 0; i < keys.size(); ++i) {
            auto        sym     = static_cast<KeySym>(keys[i].sym);
            uint32_t    state   = keys[i].state;
            std::string keyUtf8 = Key::keySymToUTF8(sym);
            if (keyUtf8.empty()) {
                continue;
            }

            bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), sym, state) != 0U;

            auto commitF = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            if (commitF && (*commitF.get() != 0)) {
                std::string commitStr = commitF.get();
                std::string deletedPart;
                std::string addedPart;
                compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

                if (!deletedPart.empty()) {
                    // Re-buffer remaining keys for next replay cycle.
                    for (size_t j = i + 1; j < keys.size(); ++j) {
                        if (buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                            buffered_keys_.push_back(keys[j]);
                        }
                    }
                    flush();
                    performReplacement(deletedPart, addedPart);
                    hasHistory_ = false;
                    ResetEngine(lotusEngine_.handle());
                    oldPreBuffer_.clear();
                    return;
                }
                if (!addedPart.empty()) {
                    out += addedPart;
                }

                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
                continue;
            }

            if (!processed) {
                out += keyUtf8;
                continue;
            }

            hasHistory_ = true;
            realtextLen.fetch_add(1, std::memory_order_acq_rel);

            UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
            std::string      preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

            std::string      deletedPart;
            std::string      addedPart;
            if (compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
                if (deletedPart.empty()) {
                    if (!addedPart.empty()) {
                        out += addedPart;
                        oldPreBuffer_ = preeditStr;
                    }
                } else {
                    if (!canSendBackspaces()) {
                        out += keyUtf8;
                        continue;
                    }

                    if (is_deleting_.load()) {
                        is_deleting_.store(false, std::memory_order_release);
                    }

                    // Re-buffer remaining keys for next replay cycle.
                    for (size_t j = i + 1; j < keys.size(); ++j) {
                        if (buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                            buffered_keys_.push_back(keys[j]);
                        }
                    }
                    flush();
                    performReplacement(deletedPart, addedPart);
                    oldPreBuffer_ = preeditStr;
                    return;
                }
            }
        }
        flush();
        NGOSEN_INFO("Replay buffered keys done");
    }

    bool LotusState::isMacroSkipModifier(KeySym sym) const {
        const auto trigger = engine_->options().macroSkipKey;
        switch (trigger) {
            case ngosen::MacroSkipKey::Shift: return sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R;
            case ngosen::MacroSkipKey::Ctrl: return sym == FcitxKey_Control_L || sym == FcitxKey_Control_R;
            case ngosen::MacroSkipKey::Alt: return sym == FcitxKey_Alt_L || sym == FcitxKey_Alt_R;
            case ngosen::MacroSkipKey::None:
            default: return false;
        }
    }

    void LotusState::handleModifierTap(const KeyEvent& keyEvent) {
        const auto trigger = engine_->options().macroSkipKey;
        if (trigger == ngosen::MacroSkipKey::None || !engine_->options().enableMacro) {
            return;
        }
        if (!isMacroSkipModifier(keyEvent.rawKey().sym())) {
            tracking_modifier_tap_ = false;
            return;
        }
        if (keyEvent.isRelease()) {
            if (tracking_modifier_tap_) {
                tracking_modifier_tap_ = false;
                macro_skip_            = true;
                EngineSetMacroEnabled(lotusEngine_.handle(), 0);
                NGOSEN_INFO("Macro skip enabled for next word");
            }
        } else {
            tracking_modifier_tap_ = true;
        }
    }

    void LotusState::cancelModifierTap() {
        tracking_modifier_tap_ = false;
    }

    void LotusState::reEnableMacroAfterWordEnd() {
        if (!macro_skip_) {
            return;
        }
        UniqueCPtr<char> preedit(EnginePullPreedit(lotusEngine_.handle()));
        if (preedit && *preedit.get() != 0) {
            return;
        }
        macro_skip_ = false;
        EngineSetMacroEnabled(lotusEngine_.handle(), engine_->options().enableMacro ? 1 : 0);
    }

    void LotusState::resetMacroSkip() {
        tracking_modifier_tap_ = false;
        macro_skip_            = false;
        if (lotusEngine_) {
            EngineSetMacroEnabled(lotusEngine_.handle(), engine_->options().enableMacro ? 1 : 0);
        }
    }
} // namespace fcitx
