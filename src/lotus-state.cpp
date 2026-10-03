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
#include "lotus-candidates.h"
#include "lotus-utils.h"
#include "lotus.h"

#include <cstddef>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>
#include <fcitx/menu.h>
#include <fcitx/userinterface.h>

#include <algorithm>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include <thread>

namespace fcitx {
    constexpr int      MAX_SCAN_LENGTH = 15;

    static inline bool isWordBreak(uint32_t ucs4) {
        // Space, tab, newline, carriage return, null, or punctuation/symbols (: ; < = > ? @)
        return ucs4 == ' ' || ucs4 == '\t' || ucs4 == '\n' || ucs4 == '\r' || ucs4 == 0 || (ucs4 >= 58 && ucs4 <= 64);
    }

    LotusState::LotusState(LotusEngine* engine, InputContext* ic) : engine_(engine), ic_(ic) {
        setEngine();
    }

    void LotusState::setEngine() {
        lotusEngine_.reset();

        if (engine_->config().inputMethod.value() == "Custom") {
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
            lotusEngine_.reset(NewEngine(engine_->config().inputMethod->data(), engine_->dictionary(), engine_->macroTable()));
        }
        setOption();
        resetMacroSkip();
    }

    void LotusState::setOption() {
        if (!lotusEngine_)
            return;
        FcitxBambooEngineOption option = {
            .autoNonVnRestore    = *engine_->config().autoNonVnRestore,
            .ddFreeStyle         = *engine_->config().ddFreeStyle,
            .macroEnabled        = *engine_->config().enableMacro,
            .autoCapitalizeMacro = *engine_->config().capitalizeMacro,
            .spellCheckWithDicts = *engine_->config().spellCheck,
            .outputCharset       = engine_->config().outputCharset->data(),
            .modernStyle         = *engine_->config().modernStyle,
            .freeMarking         = *engine_->config().freeMarking,
            .w2u                 = static_cast<int>(*engine_->config().w2u),
            .bracketTransform    = static_cast<int>(*engine_->config().bracketTransform),
            .timeFormat          = engine_->config().timeFormat->data(),
            .dateFormat          = engine_->config().dateFormat->data(),
        };

        EngineSetOption(lotusEngine_.handle(), &option);
    }

    bool LotusState::connect_uinput_server() {
        if (uinput_client_fd_ >= 0)
            return true;
        const std::string current_path = buildSocketPath("kb_socket");
        int               current_fd   = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
        if (current_fd < 0) {
            LOTUS_ERROR("Failed to create socket: " + std::string(strerror(errno)));
            return false;
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;

        addr.sun_path[0] = '\0';
        memcpy(&addr.sun_path[1], current_path.c_str(), current_path.length());
        socklen_t len = offsetof(struct sockaddr_un, sun_path) + current_path.length() + 1;

        if (connect(current_fd, (struct sockaddr*)&addr, len) == 0) {
            // The socket name is guessable, so whoever binds it first would learn word lengths.
            if (!isTrustedServerSocket(current_fd)) {
                close(current_fd);
                return false;
            }
            uinput_client_fd_ = current_fd;
            return true;
        }
        LOTUS_ERROR("Failed to connect to socket: " + std::string(strerror(errno)));
        close(current_fd);
        int old_fd = uinput_client_fd_.exchange(-1);
        if (old_fd != -1) {
            close(old_fd);
        }
        return false;
    }

    int LotusState::setup_uinput() {
        return connect_uinput_server() ? uinput_client_fd_.load(std::memory_order_acquire) : -1;
    }

    void LotusState::send_backspace_uinput(int count) const {
        if (uinput_client_fd_ < 0 && !connect_uinput_server()) {
            LOTUS_ERROR("Cannot send backspace since cannot connect to uinput server");
            return;
        }

        ssize_t n = send(uinput_client_fd_, &count, sizeof(count), MSG_NOSIGNAL);

        if (n < 0) {
            LOTUS_WARN("Failed to send backspace: " + std::string(strerror(errno)));
            int old_fd = uinput_client_fd_.exchange(-1);
            if (old_fd != -1) {
                close(old_fd);
            }
            if (connect_uinput_server()) {
                LOTUS_INFO("Reconnected to uinput server successfully");
                send(uinput_client_fd_, &count, sizeof(count), MSG_NOSIGNAL);
            }
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

        bool textAfterCursorLooksLikeUrl(const SurroundingText& s) {
            if (!s.isValid() || s.cursor() != s.anchor()) {
                return false;
            }
            const unsigned cursor = s.cursor();
            unsigned       i      = 0;
            size_t         after  = 0;
            for (uint32_t c : fcitx::utf8::MakeUTF8CharRange(s.text())) {
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
        bool onlyCurrentWordBeforeCursor(const SurroundingText& s, const std::string& buff) {
            if (!s.isValid() || buff.empty() || s.cursor() != utf8::length(buff)) {
                return false;
            }
            const std::string& t = s.text();
            return t.size() > buff.size() && t.compare(0, buff.size(), buff) == 0;
        }
    } // namespace

    bool LotusState::isAutofillCertain(const SurroundingText& s) {
        if (!s.isValid() || oldPreBuffer_.empty()) {
            return false;
        }

        auto               textRange         = fcitx::utf8::MakeUTF8CharRange(s.text());
        auto               oldPreBufferRange = fcitx::utf8::MakeUTF8CharRange(oldPreBuffer_);
        std::u32string     u32Text(textRange.begin(), textRange.end());
        std::u32string     u32OldPreBuffer(oldPreBufferRange.begin(), oldPreBufferRange.end());

        const unsigned int cursor  = s.cursor();
        const unsigned int anchor  = s.anchor();
        const size_t       textLen = u32Text.length();

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
                LOTUS_INFO("Commit: " + std::string(commit.get()));
                ic_->commitString(commit.get());
            }
        }
        ic_->inputPanel().reset();
        UniqueCPtr<char> preedit(EnginePullPreedit(lotusEngine_.handle()));
        if (preedit && (*preedit.get() != 0)) {
            std::string_view view = preedit.get();
            Text             text;
            TextFormatFlags  fmt = TextFormatFlag::NoFlag;
            if (utf8::validate(view))
                text.append(std::string(view), fmt);
            text.setCursor(static_cast<int>(text.textLength()));
            if (ic_->capabilityFlags().test(CapabilityFlag::Preedit))
                ic_->inputPanel().setClientPreedit(text);
            else
                ic_->inputPanel().setPreedit(text);
        }
        ic_->updatePreedit();
        ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void LotusState::updateEmojiPageStatus(CommonCandidateList* commonList) {
        if ((commonList == nullptr) || commonList->empty()) {
            return;
        }

        int pageSize = commonList->pageSize();
        if (pageSize <= 0) {
            pageSize = 9;
        }

        int         totalItems  = commonList->totalSize();
        int         currentPage = commonList->currentPage() + 1;
        int         totalPages  = (totalItems + pageSize - 1) / pageSize;

        std::string status = _("Page ") + std::to_string(currentPage) + "/" + std::to_string(totalPages);
        ic_->inputPanel().setAuxDown(Text(status));
    }

    void LotusState::handleEmojiMode(KeyEvent& keyEvent) {
        const KeySym currentSym      = keyEvent.rawKey().sym();
        bool         isCtrlBackspace = isBackspace(currentSym) && ((keyEvent.rawKey().states() & KeyState::Ctrl) != 0U);

        if (keyEvent.key().hasModifier() && !isCtrlBackspace) {
            keyEvent.forward();
            return;
        }

        auto baseList   = ic_->inputPanel().candidateList();
        auto commonList = std::dynamic_pointer_cast<CommonCandidateList>(baseList);
        if (commonList && currentSym >= FcitxKey_1 && currentSym <= FcitxKey_9) {
            int offset      = currentSym - FcitxKey_1;
            int globalIndex = (commonList->currentPage() * commonList->pageSize()) + offset;

            if (globalIndex < commonList->totalSize()) {
                commonList->candidateFromAll(globalIndex).select(ic_);
                keyEvent.filterAndAccept();
                return;
            }
        }

        if (commonList && !commonList->empty()) {
            int  globalCursorIndex = commonList->globalCursorIndex();
            int  totalSize         = commonList->totalSize();
            int  currentPage       = commonList->currentPage();
            int  pageSize          = commonList->pageSize();
            int  localCursorIndex  = globalCursorIndex - (currentPage * pageSize);

            bool handled = false;

            switch (currentSym) {
                case FcitxKey_Tab:
                case FcitxKey_Down: {
                    if (localCursorIndex < pageSize - 1 && globalCursorIndex < totalSize - 1) {
                        commonList->setGlobalCursorIndex(globalCursorIndex + 1);
                    } else {
                        commonList->setGlobalCursorIndex(currentPage * pageSize);
                    }
                    handled = true;
                    break;
                }

                case FcitxKey_ISO_Left_Tab:
                case FcitxKey_Up: {
                    if (localCursorIndex > 0) {
                        commonList->setGlobalCursorIndex(globalCursorIndex - 1);
                    } else {
                        int lastIndex = std::min((currentPage * pageSize) + pageSize - 1, totalSize - 1);
                        commonList->setGlobalCursorIndex(lastIndex);
                    }
                    handled = true;
                    break;
                }
                case FcitxKey_Page_Down:
                case FcitxKey_Right: {
                    if (commonList->hasNext()) {
                        commonList->next();
                        int newPage = commonList->currentPage();
                        commonList->setGlobalCursorIndex(newPage * pageSize);
                        handled = true;
                    }
                    break;
                }
                case FcitxKey_Page_Up:
                case FcitxKey_Left: {
                    if (commonList->hasPrev()) {
                        commonList->prev();
                        int newPage = commonList->currentPage();
                        commonList->setGlobalCursorIndex(newPage * pageSize);
                        handled = true;
                    }
                    break;
                }
                default: break;
            }

            if (handled) {
                updateEmojiPageStatus(commonList.get());
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
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
                if (commonList && !commonList->empty()) {
                    int globalIdx = commonList->globalCursorIndex();
                    commonList->candidateFromAll(globalIdx).select(ic_);
                    keyEvent.filterAndAccept();
                } else if (currentSym == FcitxKey_Return && !emojiBuffer_.empty()) {
                    ic_->commitString(emojiBuffer_);
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
                ic_->inputPanel().reset();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
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
                ic_->inputPanel().reset();
                ic_->updatePreedit();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                return;
            }
        } else {
            emojiCandidates_ = engine_->emojiLoader().search(emojiBuffer_);
        }

        if (!emojiBuffer_.empty()) {
            Text preeditText;
            preeditText.append(emojiBuffer_, TextFormatFlag::Underline);
            preeditText.setCursor(static_cast<int>(preeditText.textLength()));
            if (ic_->capabilityFlags().test(CapabilityFlag::Preedit))
                ic_->inputPanel().setClientPreedit(preeditText);
            else
                ic_->inputPanel().setPreedit(preeditText);
        } else {
            ic_->inputPanel().setClientPreedit(Text());
            ic_->inputPanel().setPreedit(Text());
        }

        if (!emojiCandidates_.empty()) {
            auto candidateList = std::make_unique<CommonCandidateList>();
            candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
            candidateList->setPageSize(9);

            for (size_t i = 0; i < emojiCandidates_.size(); ++i) {
                size_t localIndex = (i % 9) + 1;
                Text   displayLabel;
                if (emojiBuffer_.empty()) {
                    displayLabel.append(std::to_string(localIndex) + ": " + emojiCandidates_[i].output, TextFormatFlag::NoFlag);
                } else {
                    displayLabel.append(std::to_string(localIndex) + ": " + emojiCandidates_[i].trigger + " " + emojiCandidates_[i].output, TextFormatFlag::NoFlag);
                }
                candidateList->append(std::make_unique<EmojiCandidateWord>(displayLabel, this, emojiCandidates_[i]));
            }
            candidateList->setGlobalCursorIndex(0);

            ic_->inputPanel().setCandidateList(std::move(candidateList));
            auto currentList = std::dynamic_pointer_cast<CommonCandidateList>(ic_->inputPanel().candidateList());
            updateEmojiPageStatus(currentList.get());
        } else {
            ic_->inputPanel().setCandidateList(nullptr);
        }

        ic_->updatePreedit();
        ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    bool LotusState::deletionLooksDone() const {
        const auto& s = ic_->surroundingText();
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
            const auto waited  = (::fcitx::now(CLOCK_MONOTONIC) - surr_wait_started_at_) / 1000;
            const auto minimum = static_cast<uint64_t>(engine_->config().waitSurroundingMinPerKeyMs.value()) * static_cast<uint64_t>(std::max(expected_backspaces_, 1));
            if (waited < minimum) {
                return false;
            }
        }
        auto it = t.begin();
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
            it = utf8::nextChar(it);
        }
        const std::string before(t.begin(), it);
        auto              endsWith = [](const std::string& a, const std::string& b) { return a.size() >= b.size() && a.compare(a.size() - b.size(), b.size(), b) == 0; };
        if (!surr_wait_deleted_.empty() && endsWith(before, surr_wait_prefix_ + surr_wait_deleted_)) {
            return false; // stale snapshot: the text to delete is still there
        }
        // Messenger moves the cursor before it removes the text, so a snapshot can look done while
        // deleted chars still follow the cursor. Treat that as in progress (#267).
        if (!surr_wait_deleted_.empty() && it != t.end()) {
            const std::string charAfter(it, utf8::nextChar(it));
            std::string       passed = surr_wait_prefix_;
            for (auto d = surr_wait_deleted_.begin(); d != surr_wait_deleted_.end();) {
                const auto next = utf8::nextChar(d);
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
        bool looksLikeMessengerComposer(const SurroundingText& s) {
            if (!s.isValid()) {
                return false;
            }
            const std::string& t = s.text();
            if (t == "\n") {
                return true;
            }
            auto it = t.begin();
            for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i) {
                it = utf8::nextChar(it);
            }
            return std::string(it, t.end()) == "\n\n";
        }

        // Facebook composers (message and post box) report text ending in "\n\n" wherever the cursor
        // is, or just "\n" when empty. Check the whole field so mid-text edits match too.
        bool looksLikeFacebookComposer(const SurroundingText& s) {
            if (!s.isValid()) {
                return false;
            }
            const std::string& t = s.text();
            return t == "\n" || (t.size() >= 2 && t.compare(t.size() - 2, 2, "\n\n") == 0);
        }

        // No space or newline before the cursor: the first word of the message is being typed.
        bool isFirstWordOfMessage(const SurroundingText& s) {
            const std::string& t   = s.text();
            auto               end = t.begin();
            for (unsigned int i = 0; i < s.cursor() && end != t.end(); ++i) {
                end = utf8::nextChar(end);
            }
            return std::find_if(t.begin(), end, [](char c) { return c == ' ' || c == '\n'; }) == end;
        }
    } // namespace

    void LotusState::deliverAfterSettle(const char* reason, bool fromTimer) {
        const auto& snapshot = ic_->surroundingText();
        int         settleMs = engine_->config().waitSurroundingSettleMs.value();
        if (settleMs <= 0 || !looksLikeMessengerComposer(snapshot)) {
            finishReplacement(reason, fromTimer);
            return;
        }
        // A just-emptied composer is still reloading its placeholder, so the first word waits longer.
        if (isFirstWordOfMessage(snapshot)) {
            settleMs = std::max(settleMs, engine_->config().waitSurroundingSettleFirstWordMs.value());
        }
        // Reuse the timer-only wait state: a key arriving mid-wait finishes the wait first, focus loss
        // leaves the commit to the timer, and the snapshot watcher and timeout stay quiet.
        surr_wait_pending_       = true; // the immediate path gets here without the flag set
        surr_wait_timer_only_    = true;
        surr_wait_focus_retries_ = 0;
        settle_reason_           = reason;
        surr_wait_deliver_at_    = ::fcitx::now(CLOCK_MONOTONIC) + (static_cast<uint64_t>(settleMs) * 1000ULL);
        settle_timer_            = engine_->instance()->eventLoop().addTimeEvent(CLOCK_MONOTONIC, surr_wait_deliver_at_, 1000, [this](EventSourceTime*, uint64_t) {
            if (!surr_wait_pending_ || !surr_wait_timer_only_ || !is_deleting_.load()) {
                return false;
            }
            finishReplacement(settle_reason_, false);
            return false;
        });
    }

    void LotusState::finishReplacement(const char* reason, bool fromTimer) {
        const auto elapsedMs = (::fcitx::now(CLOCK_MONOTONIC) - surr_wait_started_at_) / 1000;
        LOTUS_INFO("Surr wait " + std::string(reason) + " after " + std::to_string(elapsedMs) + " ms");
        if (std::string(reason) == "timeout") {
            surr_snapshot_trusted_ = false;
            ++surr_timeout_streak_;
            // Frozen means at least two events, all identical to the send-time snapshot. A Firefox that
            // is merely lagging usually sends nothing or a snapshot that is still moving.
            if (surr_wait_event_count_ >= 2 && !surr_wait_saw_other_snapshot_ && surr_wait_sent_snapshot_fresh_) {
                if (++surr_frozen_streak_ >= 2 && !surr_frozen_) {
                    surr_frozen_             = true;
                    surr_frozen_probe_count_ = 0;
                    LOTUS_INFO("Surr frozen: stop waiting");
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
                LOTUS_INFO("Surr frozen: resume waiting");
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
        const bool defer = getFrontendName(ic_) == "dbus" && std::string(reason) != "focus lost";
        if (defer) {
            deferred_commit_text_    = pending_commit_string_;
            deferred_commit_pending_ = true;
            deferred_commit_timer_   = engine_->instance()->eventLoop().addTimeEvent(CLOCK_MONOTONIC, ::fcitx::now(CLOCK_MONOTONIC), 0, [this](EventSourceTime*, uint64_t) {
                flushDeferredCommit();
                return false; // never reset a timer from inside its own callback
            });
        } else if (!pending_commit_string_.empty()) {
            ic_->commitString(pending_commit_string_);
            LOTUS_INFO("Commit: " + pending_commit_string_);
        }
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        pending_commit_string_.clear();
        is_deleting_.store(false);
        if (!defer) {
            replayBufferedKeys();
        }
    }

    void LotusState::flushDeferredCommit() {
        if (!deferred_commit_pending_) {
            return;
        }
        deferred_commit_pending_ = false;
        std::string text         = std::move(deferred_commit_text_);
        deferred_commit_text_.clear();
        if (!text.empty()) {
            ic_->commitString(text);
            LOTUS_INFO("Commit (deferred): " + text);
        }
        replayBufferedKeys();
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
            // Some apps (Konsole) declare surrounding text but always send it empty; nothing can match,
            // so use the sleeping path.
            const bool emptySnapshot = ic_->surroundingText().text().empty();
            if (engine_->config().waitSurroundingEvent.value() && emptySnapshot) {
                LOTUS_INFO("Surr wait skip: empty snapshot");
            }
            bool skipFrozenWait = false;
            if (engine_->config().waitSurroundingEvent.value() && !emptySnapshot && surr_frozen_) {
                const int probeEvery = std::max(engine_->config().waitSurroundingProbeEvery.value(), 1);
                ++surr_frozen_probe_count_;
                if (surr_frozen_probe_count_ % probeEvery != 0) {
                    skipFrozenWait = true;
                }
            }
            if (engine_->config().waitSurroundingEvent.value() && !emptySnapshot && !skipFrozenWait) {
                // Sleeping blocks the single event loop, so no update could arrive. Return to the loop
                // and watch for updates from now on; a fresh watcher ignores the previous replacement's
                // late events. Keys typed meanwhile go to buffered_keys_.
                event.filterAndAccept();
                surr_wait_started_at_ = ::fcitx::now(CLOCK_MONOTONIC);
                // After a timeout the app is lagging (Firefox) and its snapshot is stale: skip the
                // immediate check.
                if (surr_snapshot_trusted_ && deletionLooksDone()) {
                    LOTUS_INFO("Skip retry");
                    deliverAfterSettle("immediate", false);
                    return true;
                }
                auto* instance                = engine_->instance();
                surr_wait_pending_            = true;
                surr_wait_event_count_        = 0;
                surr_wait_saw_other_snapshot_ = false;
                surr_wait_watcher_.reset(); // safe: we are outside its dispatch
                surr_wait_watcher_ = instance->watchEvent(EventType::InputContextSurroundingTextUpdated, EventWatcherPhase::Default, [this](Event& e) {
                    auto& ice = static_cast<InputContextEvent&>(e);
                    if (!surr_wait_pending_ || surr_wait_timer_only_ || ice.inputContext() != ic_ || !is_deleting_.load()) {
                        return;
                    }
                    // After a timeout, an early event is the app's stale buffer catching up, not the
                    // finished deletion. Ignore events before WaitSurroundingMinPerKeyMs per backspace.
                    const auto waitedUs = ::fcitx::now(CLOCK_MONOTONIC) - surr_wait_started_at_;
                    const auto minimumUs =
                        static_cast<uint64_t>(engine_->config().waitSurroundingMinPerKeyMs.value()) * static_cast<uint64_t>(std::max(expected_backspaces_, 1)) * 1000ULL;
                    {
                        const auto& current = ic_->surroundingText();
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
                });
                // Two timeouts in a row: this app does not update while deleting. Use the short timeout
                // until an event matches again.
                const int  timeoutMs = surr_timeout_streak_ >= 2 ? engine_->config().waitSurroundingShortMs.value() : engine_->config().waitSurroundingTimeoutMs.value();
                const auto timeout   = static_cast<uint64_t>(timeoutMs) * 1000ULL;
                // Accuracy 0 means sd-event's default 250 ms slack, so pass 1 ms. Check once at the
                // threshold first: many apps report "done" before it and then go quiet.
                const auto threshold =
                    static_cast<uint64_t>(engine_->config().waitSurroundingMinPerKeyMs.value()) * static_cast<uint64_t>(std::max(expected_backspaces_, 1)) * 1000ULL;
                const auto firstDeadline = threshold < timeout ? surr_wait_started_at_ + threshold : surr_wait_started_at_ + timeout;
                surr_wait_timer_         = instance->eventLoop().addTimeEvent(CLOCK_MONOTONIC, firstDeadline, 1000, [this, timeout](EventSourceTime* t, uint64_t) {
                    if (!surr_wait_pending_ || surr_wait_timer_only_ || !is_deleting_.load()) {
                        return false;
                    }
                    const auto waited = ::fcitx::now(CLOCK_MONOTONIC) - surr_wait_started_at_;
                    if (waited + 1000 < timeout) {
                        if (deletionLooksDone()) {
                            deliverAfterSettle("threshold", true);
                            return false;
                        }
                        t->setTime(surr_wait_started_at_ + timeout);
                        t->setOneShot();
                        return true;
                    }
                    finishReplacement("timeout", true);
                    return false;
                });
                return true;
            }
            // Frozen snapshot: sleep at least WaitSurroundingMinPerKeyMs x (N - 1) instead of, not on
            // top of, the normal sleep.
            const int perKeyMs = skipFrozenWait ? std::max(sleepTime, engine_->config().waitSurroundingMinPerKeyMs.value()) : sleepTime;
            int       waitMs   = perKeyMs * (expected_backspaces_ - 1);
            // Validate surr cursor pos should match realtextLen after all BS applied
            const auto& surr = ic_->surroundingText();
            if (skipFrozenWait) {
                LOTUS_INFO("Skip retry (frozen)"); // retrying 3 x 2 ms is pointless on a frozen snapshot
            } else if (surr.isValid() && surr.cursor() == realtextLen.load(std::memory_order_acquire)) {
                LOTUS_INFO("Skip retry");
            } else if (!ic_->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
                // No surrounding text capability (gnome-terminal, Chromium on X11): retrying cannot help.
                LOTUS_INFO("Skip retry (no surrounding capability)");
            } else {
                // Retry x3 (2 ms each) for apps whose snapshot is not valid yet. Use a timer, not
                // sleep_for, so the event loop can deliver a fresh snapshot.
                waitMs += 3 * 2;
            }
            event.filterAndAccept(); // Filter out the final trigger backspace.
            if (waitMs <= 0) {
                finishReplacement("immediate", false);
                return true;
            }
            // Wait on a timer rather than sleep_for, which would block the fcitx5 event loop. Keys
            // arriving meanwhile go to buffered_keys_ because is_deleting_ is set.
            surr_wait_pending_       = true;
            surr_wait_timer_only_    = true;
            surr_wait_focus_retries_ = 0;
            surr_wait_started_at_    = ::fcitx::now(CLOCK_MONOTONIC);
            surr_wait_deliver_at_    = surr_wait_started_at_ + (static_cast<uint64_t>(waitMs) * 1000ULL);
            surr_wait_timer_         = engine_->instance()->eventLoop().addTimeEvent(CLOCK_MONOTONIC, surr_wait_deliver_at_, 1000, [this](EventSourceTime* t, uint64_t) {
                if (!surr_wait_pending_ || !surr_wait_timer_only_) {
                    return false;
                }
                if (!is_deleting_.load()) { // the replacement was cancelled elsewhere (navigation key...)
                    surr_wait_pending_    = false;
                    surr_wait_timer_only_ = false;
                    return false;
                }
                if (!ic_->hasFocus()) {
                    // Chromium X11 leaves and re-enters the field within ~0.3 ms; a commit in that gap is
                    // lost. Give it a moment to come back.
                    if (++surr_wait_focus_retries_ <= 5) {
                        t->setTime(::fcitx::now(CLOCK_MONOTONIC) + 2000);
                        t->setOneShot();
                        return true;
                    }
                    // The user really switched windows: the old field can no longer take the text. Reset
                    // only this field's state; is_deleting_ is shared and the new field may be replacing.
                    LOTUS_INFO("Timer: input context lost focus, dropping text");
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
        return false;
    }

    void LotusState::send_select_uinput(int charCount) const {
        send_backspace_uinput(-charCount);
    }

    void LotusState::selectAndOvertype(const std::string& addedPart, int charCount) {
        is_deleting_.store(true, std::memory_order_release);
        pending_commit_string_   = addedPart;
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        overtype_char_count_     = charCount;
        overtype_pending_        = true;
        {
            const auto& snapshot    = ic_->surroundingText();
            overtype_had_snapshot_  = snapshot.isValid();
            overtype_cursor_before_ = overtype_had_snapshot_ ? snapshot.cursor() : 0;
        }
        overtype_started_at_ = ::fcitx::now(CLOCK_MONOTONIC);
        auto* instance       = engine_->instance();
        overtype_watcher_.reset(); // safe: we are outside its dispatch
        overtype_watcher_ = instance->watchEvent(EventType::InputContextSurroundingTextUpdated, EventWatcherPhase::Default, [this](Event& e) {
            auto& ice = static_cast<InputContextEvent&>(e);
            if (!overtype_pending_ || ice.inputContext() != ic_ || !is_deleting_.load()) {
                return;
            }
            const auto& s = ic_->surroundingText();
            if (!s.isValid()) {
                return;
            }
            const int selected = static_cast<int>(s.anchor()) - static_cast<int>(s.cursor());
            if (selected != overtype_char_count_ && selected != -overtype_char_count_) {
                return;
            }
            finishOvertype("selected", false);
        });
        overtype_timer_   = instance->eventLoop().addTimeEvent(CLOCK_MONOTONIC, overtype_started_at_ + 150000ULL, 1000, [this](EventSourceTime*, uint64_t) {
            if (overtype_pending_ && is_deleting_.load()) {
                abandonOvertype();
            }
            return false;
        });
        // forwardKey does not carry Shift into the selection, so send the keys through the uinput
        // server like a real keyboard.
        send_select_uinput(charCount);
        LOTUS_INFO("Select " + std::to_string(charCount) + " chars");
    }

    // The field did not report the selection in time. Do not type over it: the cursor has moved and
    // the text would land in the wrong place. Move the cursor back and drop this replacement; the
    // user loses one tone mark and sees it immediately.
    void LotusState::abandonOvertype() {
        const auto& snapshot     = ic_->surroundingText();
        int         rightPresses = 1; // a real but unreported selection collapses with one Right
        if (overtype_had_snapshot_ && snapshot.isValid() && snapshot.cursor() == snapshot.anchor() &&
            snapshot.cursor() + static_cast<unsigned int>(overtype_char_count_) == overtype_cursor_before_) {
            rightPresses = overtype_char_count_; // the field only moved the cursor, nothing selected
        }
        for (int i = 0; i < rightPresses; ++i) {
            ic_->forwardKey(Key(FcitxKey_Right), false);
            ic_->forwardKey(Key(FcitxKey_Right), true);
        }
        LOTUS_INFO("Overtype gave up after " + std::to_string((::fcitx::now(CLOCK_MONOTONIC) - overtype_started_at_) / 1000) + " ms, moved cursor right " +
                   std::to_string(rightPresses));
        overtype_pending_ = false;
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
        const auto elapsedMs = (::fcitx::now(CLOCK_MONOTONIC) - overtype_started_at_) / 1000;
        LOTUS_INFO("Overtype " + std::string(reason) + " after " + std::to_string(elapsedMs) + " ms");
        overtype_pending_ = false;
        if (!fromTimer && overtype_timer_) {
            overtype_timer_.reset(); // never reset a timer from inside its own callback
        }
        if (!pending_commit_string_.empty()) {
            ic_->commitString(pending_commit_string_);
            LOTUS_INFO("Commit: " + pending_commit_string_);
        }
        pending_commit_string_.clear();
        expected_backspaces_     = 0;
        current_backspace_count_ = 0;
        is_deleting_.store(false);
        replayBufferedKeys();
    }

    void LotusState::performReplacement(const std::string& deletedPart, const std::string& addedPart) {
        LOTUS_INFO("Perform replacement: " + deletedPart + " -> " + addedPart); //NOLINT
        current_backspace_count_ = 0;
        pending_commit_string_   = addedPart;
        expected_backspaces_     = static_cast<int>(utf8::length(deletedPart));
        surr_wait_deleted_       = deletedPart;
        {
            const auto& snapshot     = ic_->surroundingText();
            surr_wait_sent_snapshot_ = snapshot.isValid() ? snapshot.text() + "\x1f" + std::to_string(snapshot.cursor()) : std::string();
        }
        surr_wait_prefix_ = (oldPreBuffer_.size() >= deletedPart.size()) ? oldPreBuffer_.substr(0, oldPreBuffer_.size() - deletedPart.size()) : std::string();
        {
            // Only a fresh send-time snapshot (text before the cursor ends with prefix + deleted) may
            // count towards "frozen"; a lagging app sends a stale one.
            surr_wait_sent_snapshot_fresh_ = false;
            const auto& snapshot           = ic_->surroundingText();
            if (snapshot.isValid()) {
                const std::string& t  = snapshot.text();
                auto               it = t.begin();
                for (unsigned int i = 0; i < snapshot.cursor() && it != t.end(); ++i) {
                    it = utf8::nextChar(it);
                }
                const std::string before(t.begin(), it);
                const std::string expected     = surr_wait_prefix_ + surr_wait_deleted_;
                surr_wait_sent_snapshot_fresh_ = before.size() >= expected.size() && before.compare(before.size() - expected.size(), expected.size(), expected) == 0;
            }
        }
        const auto&       surrounding = ic_->surroundingText();
        const std::string surrText    = surrounding.text();
        // Facebook composers only: other fields do not report a selection-only change, so the
        // overtype would time out and drop the tone mark.
        if (engine_->config().messengerSelectOvertype.value() && realMode != LotusMode::Minecraft && looksLikeFacebookComposer(surrounding)) {
            selectAndOvertype(addedPart, static_cast<int>(utf8::length(deletedPart)));
            return;
        }
        // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it. Its
        // deleteSurroundingText applies at once, relative to the cursor, so use it there (#162).
        const bool isLibreOffice = ic_->program() == "soffice" && realMode != LotusMode::Minecraft;
        bool       isSurrText    = isLibreOffice ? ic_->capabilityFlags().test(CapabilityFlag::SurroundingText) :
                                                   engine_->config().useSurroundingTextIfPossible.value() && ic_->capabilityFlags().test(CapabilityFlag::SurroundingText) &&
                surrounding.isValid() && !surrText.empty() && surrounding.cursor() == utf8::length(surrText);
        if (!isSurrText && realMode != LotusMode::Minecraft) {
            ++expected_backspaces_;
            // Uinput skips the autofill guard except in address bars (#190): the Url flag on Chromium,
            // the autofill shape on Firefox.
            const bool isFirefoxAddressBar = ic_->program() == "firefox" && textAfterCursorLooksLikeUrl(surrounding);
            const bool checkAutofill       = realMode != LotusMode::Uinput || ic_->capabilityFlags().test(CapabilityFlag::Url) || isFirefoxAddressBar;
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
            ic_->deleteSurroundingText(-expected_backspaces_, expected_backspaces_);
            LOTUS_INFO("Delete using surrounding text");
            std::this_thread::sleep_for(std::chrono::milliseconds(engine_->config().surrDeleteSleepMs.value() * expected_backspaces_));
            if (!pending_commit_string_.empty()) {
                ic_->commitString(pending_commit_string_);
                LOTUS_INFO("Commit: " + pending_commit_string_);
                std::this_thread::sleep_for(std::chrono::milliseconds(engine_->config().surrCommitSleepMs.value() * utf8::length(addedPart)));
            }
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            is_deleting_.store(false);
            replayBufferedKeys();
            return;
        }
        send_backspace_uinput(expected_backspaces_);
        LOTUS_INFO("Send " + std::to_string(expected_backspaces_) + " backspaces");
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

        if (uinput_client_fd_ < 0) {
            setup_uinput();
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
                    ic_->commitString(addedPart);
                    LOTUS_INFO("Commit: " + addedPart);
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
                        ic_->commitString(addedPart);
                        LOTUS_INFO("Commit: " + addedPart);
                        keyEvent.filterAndAccept();
                        isCommit = true;
                    }
                }
                if (!isCommit) {
                    keyEvent.forward();
                }
            } else {
                if (uinput_client_fd_ < 0) {
                    LOTUS_ERROR("Cannot connect to uinput server, commit rawkey");
                    std::string rawKey = keyEvent.key().toString();
                    if (!rawKey.empty()) {
                        ic_->commitString(rawKey);
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

    void LotusState::handleSurroundingText(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }
        auto* ic = keyEvent.inputContext();
        if ((ic == nullptr) || !ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
            LOTUS_WARN("Surrounding text not supported");
            keyEvent.forward();
            return;
        }

        const auto& surrounding = ic->surroundingText();
        if (!surrounding.isValid()) {
            LOTUS_WARN("Surrounding text is invalid");
            keyEvent.forward();
            return;
        }

        if (isBackspace(keyEvent.rawKey().sym())) {
            ResetEngine(lotusEngine_.handle());
            keyEvent.forward();
            return;
        }

        const std::string& text   = surrounding.text();
        unsigned int       cursor = std::min(surrounding.anchor(), surrounding.cursor());

        size_t             textLen = utf8::lengthValidated(text);

        if (textLen == utf8::INVALID_LENGTH || cursor <= 0 || cursor > textLen) {
            processNormalKey(keyEvent, currentSym);
            return;
        }

        {
            auto startIter = utf8::nextNChar(text.begin(), cursor);
            auto endIter   = startIter;

            int  scanCount = 0;
            while (startIter != text.begin() && scanCount < MAX_SCAN_LENGTH) {
                auto prev = startIter;
                if (prev != text.begin()) {
                    --prev;
                    while (prev != text.begin() && ((*prev & 0xC0) == 0x80)) {
                        --prev;
                    }
                }

                uint32_t ucs4 = utf8::getChar(prev, text.end());

                if (isWordBreak(ucs4))
                    break;

                startIter = prev;
                ++scanCount;
            }

            std::string oldWord(startIter, endIter);

            if (oldWord.empty()) {
                processNormalKey(keyEvent, currentSym);
                return;
            }

            EngineRebuildFromText(lotusEngine_.handle(), oldWord.c_str());

            bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

            if (!processed) {
                keyEvent.forward();
                ResetEngine(lotusEngine_.handle());
                return;
            }

            auto        commitPtr  = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            auto        preeditPtr = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));

            std::string newWord;
            if (commitPtr && (*commitPtr.get() != 0))
                newWord += commitPtr.get();
            if (preeditPtr && (*preeditPtr.get() != 0))
                newWord += preeditPtr.get();

            std::string deletedPart;
            std::string addedPart;
            compareAndSplitStrings(oldWord, newWord, deletedPart, addedPart);
            if ((deletedPart.empty() || deletedPart == oldWord) && addedPart == keyEvent.key().toString()) {
                ResetEngine(lotusEngine_.handle());
                keyEvent.forward();
                return;
            }

            if (!deletedPart.empty() || !addedPart.empty()) {
                size_t charsToDelete = utf8::length(deletedPart);

                if (charsToDelete > 0) {
                    ic->deleteSurroundingText(-static_cast<int>(charsToDelete), static_cast<int>(charsToDelete));
                    std::this_thread::sleep_for(std::chrono::milliseconds(engine_->config().surrDeleteSleepMs.value() * charsToDelete));
                }

                if (!addedPart.empty()) {
                    ic->commitString(addedPart);
                    LOTUS_INFO("Commit: " + addedPart);
                }

                ResetEngine(lotusEngine_.handle());
                keyEvent.filterAndAccept();
                return;
            }

            ResetEngine(lotusEngine_.handle());
            keyEvent.filterAndAccept();
            return;
        }
    }

    void LotusState::processNormalKey(KeyEvent& keyEvent, KeySym currentSym) {
        auto* ic = keyEvent.inputContext();
        ResetEngine(lotusEngine_.handle());
        bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;
        if (processed) {
            auto        commitPtr  = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            auto        preeditPtr = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));
            std::string out;
            if (commitPtr && (*commitPtr.get() != 0))
                out += commitPtr.get();
            if (preeditPtr && (*preeditPtr.get() != 0))
                out += preeditPtr.get();

            if (!out.empty()) {
                LOTUS_INFO("Commit: " + out);
                ic->commitString(out);
            }

            ResetEngine(lotusEngine_.handle());
            keyEvent.filterAndAccept();
        } else {
            keyEvent.forward();
        }
    }

    void LotusState::handleDoubleSpaceReplacement() {
        switch (realMode) {
            case LotusMode::SurroundingText: {
                ic_->deleteSurroundingText(-1, 1);
                ic_->commitString(". ");
                LOTUS_INFO("Commit: . ");

                break;
            }
            default: { // Uinput, Preedit, etc.
                performReplacement(" ", ". ");
                LOTUS_INFO("Commit: . ");
                break;
            }
        }
        if (*engine_->config().autoCapitalizeAfterPunctuation) {
            isPrevPunctuation_ = true;
            shouldCapitalize_  = true;
        }
    }

    void LotusState::handleDoubleHyphenReplacement() {
        // Em-dash (U+2014)
        std::string emDash = "—";
        switch (realMode) {
            case LotusMode::SurroundingText: {
                ic_->deleteSurroundingText(-1, 1);
                ic_->commitString(emDash);
                LOTUS_INFO("Commit: — (em-dash)");
                break;
            }
            default: { // Uinput, Preedit, etc.
                performReplacement("-", emDash);
                LOTUS_INFO("Commit: — (em-dash)");
                break;
            }
        }
    }

    void LotusState::handleOffModeMacro(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }

        if (uinput_client_fd_ < 0) {
            connect_uinput_server();
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
                LOTUS_INFO("Macro expansion: '" + oldPreBuffer_ + "' -> '" + commitStr + "'");
                // Try uinput replacement first, fallback to deleteSurroundingText, then plain commit
                if (uinput_client_fd_ >= 0 && !oldPreBuffer_.empty()) {
                    performReplacement(oldPreBuffer_, commitStr);
                } else if (ic_->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
                    const auto& surrounding = ic_->surroundingText();
                    if (surrounding.isValid()) {
                        size_t oldLen = utf8::length(oldPreBuffer_);
                        if (oldLen > 0) {
                            ic_->deleteSurroundingText(-static_cast<int>(oldLen), static_cast<int>(oldLen));
                        }
                        ic_->commitString(commitStr);
                    } else {
                        ic_->commitString(commitStr);
                    }
                } else {
                    ic_->commitString(commitStr);
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
        if (uinput_client_fd_ < 0) {
            LOTUS_WARN("Cannot connect to uinput server, reconnecting....");
            connect_uinput_server();
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
            LOTUS_INFO("Need engine reset");
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
        if (*engine_->config().autoCapitalizeAfterPunctuation && realMode != LotusMode::Off) {
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

        if (is_deleting_.load(std::memory_order_acquire) && surr_wait_timer_only_) {
            // A key arrived during a timer-only wait. Replaying it via commitString loses text on
            // Chromium X11, so finish the wait, commit, then handle the key normally.
            const uint64_t nowUs = ::fcitx::now(CLOCK_MONOTONIC);
            if (surr_wait_deliver_at_ > nowUs) {
                std::this_thread::sleep_for(std::chrono::microseconds(surr_wait_deliver_at_ - nowUs));
            }
            finishReplacement("key arrived", false);
        }
        if (is_deleting_.load(std::memory_order_acquire)) {
            if (isBackspace(currentSym)) {
                if (realtextLen.load(std::memory_order_acquire) > 0)
                    realtextLen.fetch_sub(1, std::memory_order_acq_rel);
                if (handleUInputKeyPress(keyEvent, currentSym, realMode == LotusMode::Uinput ? 4 : 8)) {
                    return;
                }
            } else {
                std::string keyUtf8Check = Key::keySymToUTF8(currentSym);
                if (!keyUtf8Check.empty() && buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                    LOTUS_WARN("Typing so fast, add key to queue");
                    buffered_keys_.push_back({.sym = currentSym, .state = keyEvent.rawKey().states()});
                }
                keyEvent.filterAndAccept();
            }
            return;
        }

        if (*engine_->config().doubleSpaceToPeriod && realMode != LotusMode::Off) {
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

        if (*engine_->config().doubleHyphenToEmDash && realMode != LotusMode::Off) {
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
            case LotusMode::Minecraft:
            case LotusMode::Uinput: {
                handleUinputMode(keyEvent, currentSym);
                break;
            }
            case LotusMode::SurroundingText: {
                handleSurroundingText(keyEvent, currentSym);
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
                if (*engine_->config().enableMacroInOffMode && *engine_->config().enableMacro) {
                    handleOffModeMacro(keyEvent, currentSym);
                }
                break;
            }
        }
        reEnableMacroAfterWordEnd();
    }

    void LotusState::reset(bool isFocusOut) {
        const auto& surrounding = ic_->surroundingText();
        const auto& text        = surrounding.text();
        size_t      textLen     = utf8::length(text);
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
                    ic_->commitString(commit.get());
                    LOTUS_INFO("Commit: " + std::string(commit.get()));
                }
            }
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
        }
        if (getFrontendName(ic_) != "dbus")
            clearAllBuffers();

        switch (realMode) {
            case LotusMode::Preedit: {
                ic_->inputPanel().reset();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                ic_->updatePreedit();
                break;
            }
            case LotusMode::SurroundingText:
            case LotusMode::Minecraft:
            case LotusMode::Uinput: {
                ic_->inputPanel().reset();
                break;
            }
            case LotusMode::Emoji: {
                ic_->inputPanel().reset();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                ic_->updatePreedit();
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
                ic_->inputPanel().reset();
                if (lotusEngine_) {
                    EngineCommitPreedit(lotusEngine_.handle());
                    UniqueCPtr<char> commit(EnginePullCommit(lotusEngine_.handle()));
                    if (commit && (*commit.get() != 0))
                        ic_->commitString(commit.get());
                    ResetEngine(lotusEngine_.handle());
                }
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                ic_->updatePreedit();
                break;
            }
            case LotusMode::SurroundingText:
            case LotusMode::Minecraft:
            case LotusMode::Uinput: {
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
        LOTUS_DEBUG("Clear all buffers");
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

    void LotusState::replayBufferedKeys() {
        LOTUS_INFO("Starting replay buffered keys");
        if (buffered_keys_.empty()) {
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
                    performReplacement(deletedPart, addedPart);
                    hasHistory_ = false;
                    ResetEngine(lotusEngine_.handle());
                    oldPreBuffer_.clear();
                    return;
                }
                if (!addedPart.empty()) {
                    ic_->commitString(addedPart);
                }

                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
                continue;
            }

            if (!processed) {
                ic_->commitString(keyUtf8);
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
                        ic_->commitString(addedPart);
                        oldPreBuffer_ = preeditStr;
                    }
                } else {
                    if (uinput_client_fd_ < 0) {
                        ic_->commitString(keyUtf8);
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
                    performReplacement(deletedPart, addedPart);
                    oldPreBuffer_ = preeditStr;
                    return;
                }
            }
        }
        LOTUS_INFO("Replay buffered keys done");
    }

    bool LotusState::isMacroSkipModifier(KeySym sym) const {
        const auto trigger = engine_->config().macroSkipTriggerModifier.value();
        switch (trigger) {
            case MacroSkipTriggerModifier::Shift: return sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R;
            case MacroSkipTriggerModifier::Ctrl: return sym == FcitxKey_Control_L || sym == FcitxKey_Control_R;
            case MacroSkipTriggerModifier::Alt: return sym == FcitxKey_Alt_L || sym == FcitxKey_Alt_R;
            case MacroSkipTriggerModifier::Disabled:
            default: return false;
        }
    }

    void LotusState::handleModifierTap(const KeyEvent& keyEvent) {
        const auto trigger = engine_->config().macroSkipTriggerModifier.value();
        if (trigger == MacroSkipTriggerModifier::Disabled || !*engine_->config().enableMacro) {
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
                LOTUS_INFO("Macro skip enabled for next word");
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
        EngineSetMacroEnabled(lotusEngine_.handle(), *engine_->config().enableMacro ? 1 : 0);
    }

    void LotusState::resetMacroSkip() {
        tracking_modifier_tap_ = false;
        macro_skip_            = false;
        if (lotusEngine_) {
            EngineSetMacroEnabled(lotusEngine_.handle(), *engine_->config().enableMacro ? 1 : 0);
        }
    }
} // namespace fcitx
