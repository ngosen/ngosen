/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "ngosen-state.h"
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <thread>

namespace ngosen {

    // Chromium asks the input method about each key before it handles the key, so our Shift release
    // coming back does not mean the Left presses before it have moved the selection yet.
    constexpr uint64_t XTestSelectSettleUs = 50000;

    TypingState::TypingState(ngosen::EngineResources* engine, std::unique_ptr<ngosen::Host> host) :
        engine_(engine), host_(ngosen::recordingHost(std::move(host), engine->recorder())) {
        setEngine();
    }

    void TypingState::setEngine() {
        bambooEngine_.reset();
        bambooEngine_.reset(newBambooEngine());
        setOption();
        tracking_modifier_tap_ = false;
        lock_word_pending_     = false;
    }

    uintptr_t TypingState::newBambooEngine() const {
        if (engine_->options().inputMethod == "Custom") {
            const auto         keymaps = engine_->customKeymap();
            std::vector<char*> charArray;
            charArray.reserve((keymaps.size() * 2) + 1);
            for (const auto& keymap : keymaps) {
                charArray.push_back(const_cast<char*>(keymap.key.data()));   //NOLINT
                charArray.push_back(const_cast<char*>(keymap.value.data())); //NOLINT
            }
            charArray.push_back(nullptr);
            return NewCustomEngine(charArray.data(), engine_->dictionary(), engine_->macroTable());
        }
        return NewEngine(engine_->options().inputMethod.data(), engine_->dictionary(), engine_->macroTable());
    }

    void TypingState::setOption() {
        if (bambooEngine_)
            setOption(bambooEngine_.handle());
    }

    void TypingState::setOption(uintptr_t engine) const {
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

        EngineSetOption(engine, &option);
    }

    void TypingState::handlePreeditMode(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (ngosen::key::isBackspace(currentSym) && engine_->options().backspaceRemovesTone) {
            ngosen::UniqueCPtr<char> preedit(EnginePullPreedit(bambooEngine_.handle()));
            const std::string        shown   = preedit ? preedit.get() : "";
            const std::string        untoned = withoutToneMark(shown);
            if (untoned != shown) {
                EngineRebuildFromText(bambooEngine_.handle(), untoned.c_str());
                keyEvent.accept();
                host_->showPreedit(untoned, false);
                host_->refreshPreedit();
                return;
            }
        }
        const bool engineTook = EngineProcessKeyEvent(bambooEngine_.handle(), currentSym, keyEvent.states()) != 0U;
        if (engineTook)
            keyEvent.accept();
        if (auto pulled = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()))) {
            if (pulled && (*pulled.get() != 0)) {
                NGOSEN_INFO("Commit: " + logged(pulled.get()));
                commit(pulled.get());
                if ((currentSym == ngosen::key::Return || currentSym == ngosen::key::KP_Enter) && !engineTook && !keyEvent.hasModifier())
                    holdEnterAfterCommit(keyEvent);
            }
        }
        host_->resetPanel();
        ngosen::UniqueCPtr<char> preedit(EnginePullPreedit(bambooEngine_.handle()));
        if (preedit && (*preedit.get() != 0)) {
            std::string_view view = preedit.get();
            host_->showPreedit(ngosen::utf8::validate(view) ? std::string(view) : std::string(), false);
        }
        host_->refreshPreedit();
        host_->refreshPanel();
    }

    void TypingState::holdEnterAfterCommit(ngosen::KeyPress& keyEvent) {
        const Field field = host_->field();
        if (!ngosen::forwardsBackspaces(field) || ngosen::ignoresForwardedKeys(field))
            return;
        keyEvent.accept();
        held_enter_pending_ = true;
        held_enter_release_ = true;
        held_enter_timer_   = host_->startTimer(ngosen::monotonicUs() + EnterAfterCommitUs, 1000, [this](ngosen::Timer&) {
            if (held_enter_pending_) {
                held_enter_pending_ = false;
                forwardTap(ngosen::EditKey::Return);
            }
            return false;
        });
    }

    void TypingState::sendHeldEnter() {
        if (!held_enter_pending_)
            return;
        held_enter_pending_ = false;
        held_enter_timer_.reset();
        forwardTap(ngosen::EditKey::Return);
    }

    bool TypingState::checkForwardSpecialKey(ngosen::KeyPress& keyEvent, uint32_t& currentSym) {
        if (keyEvent.isCursorMove() || currentSym == ngosen::key::Tab || currentSym == ngosen::key::KP_Tab || currentSym == ngosen::key::ISO_Left_Tab ||
            currentSym == ngosen::key::Escape || keyEvent.hasModifier()) {
            is_deleting_.store(false, std::memory_order_release);
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            hasHistory_ = false;
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            return true;
        }

        if (currentSym == ngosen::key::Delete) {
            return true;
        }

        if (currentSym >= ngosen::key::KP_0 && currentSym <= ngosen::key::KP_9) {
            currentSym = ngosen::key::Digit0 + (currentSym - ngosen::key::KP_0);
            return false;
        }

        switch (currentSym) {
            case ngosen::key::KP_Add: {
                currentSym = ngosen::key::plus;
                break;
            }
            case ngosen::key::KP_Subtract: {
                currentSym = ngosen::key::minus;
                break;
            }
            case ngosen::key::KP_Divide: {
                currentSym = ngosen::key::slash;
                break;
            }
            case ngosen::key::KP_Multiply: {
                currentSym = ngosen::key::asterisk;
                break;
            }
            case ngosen::key::KP_Decimal: {
                currentSym = ngosen::key::period;
                break;
            }
            case ngosen::key::KP_Enter: {
                currentSym = ngosen::key::Return;
                break;
            }
            case ngosen::key::KP_Equal: {
                currentSym = ngosen::key::equal;
                break;
            }
            case ngosen::key::KP_Space: {
                currentSym = ngosen::key::space;
                break;
            }
            default: break;
        }
        return false;
    }

    // fcitx gives BackSpace, Return and Tab their control characters, which are not text to commit.
    bool TypingState::typesText(const ngosen::KeyPress& keyEvent, uint32_t currentSym) const {
        const std::string text = host_->keyText(currentSym);
        return !text.empty() && static_cast<unsigned char>(text[0]) >= 0x20 && text[0] != '\x7f' &&
            (keyEvent.states() & (ngosen::modifier::Ctrl | ngosen::modifier::Alt | ngosen::modifier::Super)) == 0U;
    }

    bool TypingState::typedBeforeCommitArrived(const ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (keyEvent.isRelease() || commitNotSeen_.empty())
            return false;
        // A report of our deletion can come after the commit, so only the committed text itself shows it arrived.
        if (!ngosen::appliesEarlyKeysFirst(host_->field()) || !typesText(keyEvent, currentSym) || typedBeforeCursor(commitNotSeen_)) {
            commitNotSeen_.clear();
            return false;
        }
        return true;
    }

    void TypingState::handleSenMode(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.passToApp();
            return;
        }

        if (ngosen::key::isBackspace(currentSym) && engine_->options().backspaceRemovesTone && removeToneMark(keyEvent))
            return;

        if (ngosen::key::isBackspace(currentSym) || currentSym == ngosen::key::Return) {
            if (ngosen::key::isBackspace(currentSym)) {
                hasHistory_ = true;
                if (oldPreBuffer_.empty() && !lastWord_.empty())
                    reopenLastWord();
                else
                    EngineProcessKeyEvent(bambooEngine_.handle(), ngosen::key::BackSpace, 0);
                ngosen::UniqueCPtr<char> preeditC(EnginePullPreedit(bambooEngine_.handle()));
                oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            } else {
                hasHistory_ = false;
                ResetEngine(bambooEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.passToApp();
            return;
        }

        std::string keyUtf8 = host_->keyText(currentSym);
        if (keyUtf8.empty()) {
            keyEvent.passToApp();
            return;
        }

        const bool startsWord = oldPreBuffer_.empty();
        bool       processed  = EngineProcessKeyEvent(bambooEngine_.handle(), currentSym, keyEvent.states()) != 0U;

        auto       commitF = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()));
        if (commitF && (*commitF.get() != 0)) {
            commitWord(keyEvent, currentSym, keyUtf8, commitF.get());
            return;
        }
        if (startsWord && currentSym == keyEvent.sym())
            noteRetoneKey(currentSym, keyEvent.states(), keyUtf8);

        if (!processed) {
            ngosen::UniqueCPtr<char> preeditC(EnginePullPreedit(bambooEngine_.handle()));
            if (!preeditC || (*preeditC.get() == 0)) {
                hasHistory_ = false;
                ResetEngine(bambooEngine_.handle());
                oldPreBuffer_.clear();
                keyEvent.passToApp();
            }
            return;
        }

        replaceFromPreedit(keyEvent, currentSym, keyUtf8);
    }

    bool TypingState::removeToneMark(ngosen::KeyPress& keyEvent) {
        const std::string untoned = withoutToneMark(oldPreBuffer_);
        if (untoned == oldPreBuffer_ || !canSendBackspaces())
            return false;

        std::string deletedPart;
        std::string addedPart;
        ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, untoned, deletedPart, addedPart);
        EngineRebuildFromText(bambooEngine_.handle(), untoned.c_str());
        is_deleting_.store(false, std::memory_order_release);
        hasHistory_ = true;
        keyEvent.accept();
        performReplacement(deletedPart, addedPart);
        oldPreBuffer_ = untoned;
        return true;
    }

    // Plain ASCII letters are written the same in every charset, so the engine can rebuild them.
    static bool isPlainLetters(std::string committed) {
        ngosen::utf8::eraseLastCodepoint(committed);
        return std::all_of(committed.begin(), committed.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); });
    }

    void TypingState::reopenLastWord() {
        // The user may have clicked elsewhere; only an app that reports its text can tell us.
        if (!typedBeforeCursor(lastWord_, true) || !ngosen::utf8::validate(lastWord_)) {
            lastWord_.clear();
            return;
        }
        ngosen::utf8::eraseLastCodepoint(lastWord_);
        const std::u32string chars   = ngosen::utf8::decode(lastWord_);
        const auto           letters = static_cast<size_t>(std::find_if_not(chars.rbegin(), chars.rend(), isWordChar) - chars.rbegin());
        if (letters == 0)
            return;
        auto start = lastWord_.cbegin();
        for (size_t i = letters; i < chars.size(); ++i)
            start = ngosen::utf8::nextChar(start, lastWord_.cend());
        const std::string word(start, lastWord_.cend());
        lastWord_.clear();
        EngineRebuildFromText(bambooEngine_.handle(), word.c_str());
    }

    void TypingState::commitWord(ngosen::KeyPress& keyEvent, uint32_t currentSym, const std::string& keyUtf8, const std::string& commitStr) {
        std::string deletedPart;
        std::string addedPart;
        ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

        if (!deletedPart.empty()) {
            performReplacement(deletedPart, addedPart);
            keyEvent.accept();
        } else {
            bool wasAutoCapitalized = (currentSym != keyEvent.sym());
            if (!addedPart.empty() && (keyUtf8 != addedPart || wasAutoCapitalized)) {
                // The key is accepted, so the commit carries its text (#178).
                commit(addedPart);
                NGOSEN_INFO("Commit: " + logged(addedPart));
                keyEvent.accept();
            } else {
                keyEvent.passToApp();
            }
        }

        hasHistory_ = false;
        ResetEngine(bambooEngine_.handle());
        oldPreBuffer_.clear();
        // The engine rebuilds a word from Unicode, not from a legacy charset's encoding of it.
        const bool rebuildable = engine_->options().outputCharset == "Unicode" || isPlainLetters(commitStr);
        lastWord_              = rebuildable ? commitStr : std::string();
    }

    void TypingState::replaceFromPreedit(ngosen::KeyPress& keyEvent, uint32_t currentSym, const std::string& keyUtf8) {
        hasHistory_ = true;
        realtextLen.fetch_add(1, std::memory_order_acq_rel);

        ngosen::UniqueCPtr<char> preeditC(EnginePullPreedit(bambooEngine_.handle()));
        std::string              preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

        std::string              deletedPart;
        std::string              addedPart;

        if (ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
            if (deletedPart.empty()) {
                bool isCommit           = false;
                bool wasAutoCapitalized = (currentSym != keyEvent.sym());
                if (!addedPart.empty()) {
                    oldPreBuffer_ = preeditStr;
                    if (wasAutoCapitalized || addedPart != keyUtf8) {
                        commit(addedPart);
                        NGOSEN_INFO("Commit: " + logged(addedPart));
                        keyEvent.accept();
                        isCommit = true;
                    }
                }
                if (!isCommit) {
                    passed_key_ = live_key_;
                    keyEvent.passToApp();
                }
            } else {
                if (!canSendBackspaces()) {
                    NGOSEN_ERROR("Cannot send backspaces here, commit rawkey");
                    std::string rawKey = keyEvent.name();
                    if (!rawKey.empty()) {
                        commit(rawKey);
                    }
                    return;
                }

                if (is_deleting_.load()) {
                    is_deleting_.store(false, std::memory_order_release);
                }

                keyEvent.accept();
                performReplacement(deletedPart, addedPart);
                oldPreBuffer_ = preeditStr;
            }
        }
    }

    void TypingState::recordKey(const ngosen::KeyPress& keyEvent) {
        static const char* const modeNames[] = {"Off", "Sen", "Preedit", "Emoji"};
        const auto               f           = host_->field();
        const std::string        program     = f.reportedProgram.empty() ? f.program : f.program + " (" + f.reportedProgram + ")";
        std::string field = "frontend=" + f.frontend + " program=" + program + " surrounding=" + (f.surroundingText ? "1" : "0") + " preedit=" + (f.preedit ? "1" : "0") +
            (f.password ? " password=1" : "") + " mode=" + modeNames[static_cast<int>(realMode.load())];
        auto& recorder = engine_->recorder();
        recorder.addField(field);
        if (f.password) {
            recorder.add("key", "hidden");
            return;
        }
        char sym[16];
        std::snprintf(sym, sizeof(sym), "0x%04x", keyEvent.sym());
        std::string detail = std::string(keyEvent.isRelease() ? "up " : "down ") + sym;
        if (const auto text = host_->keyText(keyEvent.sym()); !text.empty() && text != " ")
            detail += " " + text;
        if (keyEvent.states() != 0)
            detail += " states=" + std::to_string(keyEvent.states());
        if (g_mouse_clicked.load(std::memory_order_acquire))
            detail += " after-click";
        recorder.add("key", detail);
    }

    void TypingState::keyEvent(ngosen::KeyPress& keyEvent) {
        handleKey(keyEvent);
        if (!keyEvent.isRelease() && !keyEvent.isModifier()) {
            keySinceReport_ = true;
            reportAtKey_    = host_->surrounding();
        }
        // A report with the cursor at the end of the text tells how long the text is.
        const ngosen::Surrounding s = host_->surrounding();
        if (ngosen::utf8::length(s.text()) == s.cursor())
            realtextLen.store(s.cursor(), std::memory_order_release);
    }

    void TypingState::handleKey(ngosen::KeyPress& keyEvent) {
        recordKey(keyEvent);
        if (keyEvent.sym() == ngosen::key::Return || keyEvent.sym() == ngosen::key::KP_Enter) {
            const bool heldRelease = keyEvent.isRelease() && held_enter_release_;
            held_enter_release_    = false;
            if (heldRelease) {
                keyEvent.accept();
                return;
            }
        }
        if (!keyEvent.isRelease())
            sendHeldEnter();
        if (!keyEvent.isRelease() && !keyEvent.isModifier())
            dropRetoneKey();
        if (!bambooEngine_ || (retype_ && handleRetypeKey(keyEvent)) || skipsKey(keyEvent))
            return;
        settleBeforeKey();
        const uint32_t currentSym = trackCapitalization(keyEvent);
        // Only plain backspaces typed after a finished commit may reopen the word; anything else changes the text after it.
        // Backspaces we press through XTEST come back here while we delete; they belong to the commit.
        const bool deleting     = is_deleting_.load(std::memory_order_acquire);
        const bool ownBackspace = deleting && ngosen::key::isBackspace(currentSym) && !ngosen::forwardsBackspaces(host_->field());
        if (!ngosen::key::isBackspace(currentSym) || keyEvent.hasModifier() || (deleting && !ownBackspace))
            lastWord_.clear();
        if (handleKeyDuringReplacement(keyEvent, currentSym) || handleDoubledKey(keyEvent, currentSym))
            return;

        // Vietnamese typed since then, so the English noted is no longer right before the cursor.
        if (realMode != ngosen::Mode::Off)
            forgetKeysWhileOff();
        switch (realMode) {
            case ngosen::Mode::Sen: {
                const bool beforeCommit = typedBeforeCommitArrived(keyEvent, currentSym);
                live_key_               = KeyEntry{.sym = currentSym, .state = keyEvent.states()};
                passed_key_.reset();
                handleSenMode(keyEvent, currentSym);
                live_key_.reset();
                // Firefox applies a key it let through ahead of a commit it already has; a commit keeps the order.
                if (beforeCommit && !keyEvent.accepted()) {
                    keyEvent.accept();
                    passed_key_.reset();
                    commit(host_->keyText(currentSym));
                }
                break;
            }
            case ngosen::Mode::Preedit: {
                handlePreeditMode(keyEvent, currentSym);
                break;
            }
            case ngosen::Mode::Emoji: {
                handleEmojiMode(keyEvent);
                break;
            }
            default: {
                noteKeyWhileOff(keyEvent);
                if (engine_->options().enableMacroInOffMode && engine_->options().enableMacro) {
                    handleOffModeMacro(keyEvent, currentSym);
                }
                break;
            }
        }
    }

    bool TypingState::skipsKey(ngosen::KeyPress& keyEvent) {
        if (overtype_pending_ && overtype_via_xtest_ && keyEvent.sym() == ngosen::key::Shift_R) {
            keyEvent.passToApp();
            if (keyEvent.isRelease() && !overtype_shift_released_ && overtype_timer_) {
                overtype_shift_released_ = true;
                overtype_timer_->rearm(ngosen::monotonicUs() + XTestSelectSettleUs);
            }
            return true;
        }
        if (keyEvent.isModifier()) {
            handleModifierTap(keyEvent);
            if (realMode != ngosen::Mode::Preedit || keyEvent.isBareShift())
                return true;
        } else {
            cancelModifierTap();
        }
        if (keyEvent.isRelease())
            return true;
        const uint64_t lastPressAtUs = lastInputAtUs_;
        lastInputAtUs_               = ngosen::monotonicUs();
        // An app may send a key we let through back to us: XIM clients as is, gnome-text-editor at once
        // and marked as repeated; let it through rather than count it twice. Some frontends stamp a held
        // key's repeats with its first second. Our XTEST backspaces share one time, and XIM marks them repeats.
        const bool ownBackspace = is_deleting_.load(std::memory_order_acquire) && ngosen::key::isBackspace(keyEvent.sym()) && !ngosen::forwardsBackspaces(host_->field());
        const bool sameAsLast   = keyEvent.time() != 0 && keyEvent.time() == lastPressTime_ && keyEvent.code() == lastPressCode_;
        const bool resentNow    = !ownBackspace && (keyEvent.states() & ngosen::modifier::Repeat) != 0 && lastInputAtUs_ - lastPressAtUs < SentBackWithinUs;
        const bool sentBack     = resentNow || (!is_deleting_.load(std::memory_order_acquire) && host_->field().frontend == "xim");
        if (sameAsLast && sentBack) {
            NGOSEN_INFO("App sent a key back: " + logged(keyEvent.name()));
            keyEvent.passToApp();
            return true;
        }
        lastPressCode_      = keyEvent.code();
        lastPressTime_      = keyEvent.time();
        forwardsSincePress_ = 0;
        if (const uint32_t rawSym = keyEvent.sym(); overtype_pending_ && (rawSym == ngosen::key::Left || rawSym == ngosen::key::Shift_L || rawSym == ngosen::key::Shift_R)) {
            // Our own Shift+Left selection: it must reach the app and must not be treated as the user
            // moving the cursor (that would discard the pending commit).
            keyEvent.passToApp();
            return true;
        }
        return false;
    }

    void TypingState::forwardTap(ngosen::EditKey key) {
        // GLFW under XIM (Minecraft) drops a key press that is not newer than the last press of that key.
        const uint32_t time = lastPressTime_ + ++forwardsSincePress_;
        host_->forwardKey(key, false, time);
        host_->forwardKey(key, true, time);
    }

    void TypingState::settleBeforeKey() {
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
        if (needEngineReset.load() && realMode != ngosen::Mode::Off) {
            NGOSEN_INFO("Need engine reset");
            oldPreBuffer_.clear();
            hasHistory_ = false;
            ResetEngine(bambooEngine_.handle());
            is_deleting_.store(false);
            current_backspace_count_ = 0;
            isPrevSpace_             = false;
            shouldCapitalize_        = false;
            isPrevPunctuation_       = false;
            needEngineReset.store(false);
        }

        if (lock_word_pending_ && !is_deleting_.load(std::memory_order_acquire))
            lockWord();

        // A key that beats the deferred commit must not land before it.
        flushDeferredCommit();
        if (g_mouse_clicked.load(std::memory_order_acquire) && !is_deleting_.load(std::memory_order_acquire)) {
            g_mouse_clicked.store(false, std::memory_order_release);
            clearAllBuffers();
            forgetKeysWhileOff();
        }
    }

    bool TypingState::atStartOfLine() const {
        const Field field = host_->field();
        // Only a field that reports its text can tell. A typed key arrives after the report of the key
        // before it, even over IBus; a key since the last report is caught below.
        if (!field.surroundingText || field.url || field.password)
            return false;
        // GTK over Wayland reports the text from before a paste or a new line it inserts itself.
        if (keySinceReport_)
            return false;
        // The app does not hold a word still being typed, so ask the engine whether one is.
        const ngosen::UniqueCPtr<char> word(EnginePullPreedit(bambooEngine_.handle()));
        if (word && *word.get() != 0)
            return false;
        const Surrounding s = host_->surrounding();
        if (!s.isValid())
            return false;
        const std::string& t  = s.text();
        auto               it = t.begin();
        for (unsigned int i = 0; i < s.cursor() && it != t.end(); ++i)
            it = ngosen::utf8::nextChar(it, t.end());
        std::string before(t.begin(), it);
        while (!before.empty() && (before.back() == ' ' || before.back() == '\t'))
            before.pop_back();
        return before.empty() || before.back() == '\n';
    }

    uint32_t TypingState::trackCapitalization(ngosen::KeyPress& keyEvent) {
        uint32_t       currentSym = keyEvent.sym();
        const Options& options    = engine_->options();
        if ((!options.autoCapitalizeAfterPunctuation && !options.capitalizeStartOfText) || realMode == ngosen::Mode::Off)
            return currentSym;
        // Ignore auto-capitalize side-effects if we're processing automated replacement backspaces
        if (is_deleting_.load(std::memory_order_acquire) && ngosen::key::isBackspace(currentSym))
            return currentSym;

        if (!shouldCapitalize_ && options.capitalizeStartOfText && !keyEvent.hasModifier() && currentSym >= ngosen::key::a && currentSym <= ngosen::key::z)
            shouldCapitalize_ = atStartOfLine();
        if (shouldCapitalize_) {
            if (currentSym >= ngosen::key::a && currentSym <= ngosen::key::z) {
                auto upperSym = currentSym - (ngosen::key::a - ngosen::key::A);
                currentSym    = upperSym;
                keyEvent.replaceSym(upperSym);
                shouldCapitalize_ = false;
            } else if (currentSym != ngosen::key::space) {
                shouldCapitalize_ = false;
            }
        }

        if (!options.autoCapitalizeAfterPunctuation)
            return currentSym;
        switch (currentSym) {
            case ngosen::key::period:
            case ngosen::key::exclam:
            case ngosen::key::question: isPrevPunctuation_ = true; break;
            case ngosen::key::Return:
            case ngosen::key::KP_Enter:
                shouldCapitalize_  = true;
                isPrevPunctuation_ = false;
                break;
            case ngosen::key::space:
                if (isPrevPunctuation_) {
                    shouldCapitalize_  = true;
                    isPrevPunctuation_ = false;
                }
                break;
            default: isPrevPunctuation_ = false; break;
        }
        return currentSym;
    }

    bool TypingState::handleKeyDuringReplacement(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (is_deleting_.load(std::memory_order_acquire) && surr_wait_timer_only_ && ngosen::key::isBackspace(currentSym) && host_->field().frontend == "xim" &&
            ngosen::forwardsBackspaces(host_->field())) {
            // The XIM client handed a forwarded backspace back unprocessed. Let it through so the
            // client applies it, and commit after it.
            NGOSEN_INFO("XIM handed back a forwarded backspace");
            deferTimedCommit(ngosen::monotonicUs() + ForwardWaitUs);
            return true;
        }
        if (is_deleting_.load(std::memory_order_acquire) && surr_wait_timer_only_) {
            // A key arrived during a timer-only wait. Replaying it via commitString loses text on
            // Chromium X11, so finish the wait, commit, then handle the key normally.
            const uint64_t nowUs = ngosen::monotonicUs();
            if (surr_wait_deliver_at_ > nowUs) {
                std::this_thread::sleep_for(std::chrono::microseconds(surr_wait_deliver_at_ - nowUs));
            }
            // A dbus client gets the commit after this key's reply and may type a key let through
            // first, so a typing key goes out inside the commit.
            if (host_->field().frontend == "dbus" && typesText(keyEvent, currentSym) && buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                buffered_keys_.push_back({.sym = currentSym, .state = keyEvent.states()});
                keyEvent.accept();
                finishReplacement("key arrived", false);
                return true;
            }
            finishReplacement("key arrived", false);
        }
        if (is_deleting_.load(std::memory_order_acquire) && ngosen::key::isBackspace(currentSym) && ngosen::forwardsBackspaces(host_->field()) && surr_wait_pending_) {
            // Forwarded backspaces never come back, so this one is the user's: finish the replacement
            // first, then handle it normally.
            finishReplacement("backspace arrived", false);
        }
        if (!is_deleting_.load(std::memory_order_acquire))
            return false;
        if (ngosen::key::isBackspace(currentSym)) {
            if (realtextLen.load(std::memory_order_acquire) > 0)
                realtextLen.fetch_sub(1, std::memory_order_acq_rel);
            handleReplacementBackspace(keyEvent, currentSym, 4);
        } else {
            std::string keyUtf8Check = host_->keyText(currentSym);
            if (!keyUtf8Check.empty() && buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                NGOSEN_WARN("Typing so fast, add key to queue");
                buffered_keys_.push_back({.sym = currentSym, .state = keyEvent.states()});
            }
            keyEvent.accept();
        }
        return true;
    }

    bool TypingState::handleDoubledKey(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (realMode == ngosen::Mode::Off)
            return false;
        if (engine_->options().doubleSpaceToPeriod) {
            bool isSpaceKey = (currentSym == ngosen::key::space || currentSym == ngosen::key::KP_Space);
            if (isSpaceKey && !keyEvent.hasModifier()) {
                if (isPrevSpace_) {
                    keyEvent.accept();
                    handleDoubleSpaceReplacement();
                    isPrevSpace_ = false;
                    return true;
                }
                isPrevSpace_ = true;
            } else {
                isPrevSpace_ = false;
            }
        }

        if (engine_->options().doubleHyphenToEmDash) {
            bool isHyphenKey = (currentSym == ngosen::key::minus || currentSym == ngosen::key::KP_Subtract);
            if (isHyphenKey && !keyEvent.hasModifier()) {
                if (isPrevHyphen_) {
                    keyEvent.accept();
                    handleDoubleHyphenReplacement();
                    isPrevHyphen_ = false;
                    return true;
                }
                isPrevHyphen_ = true;
            } else {
                isPrevHyphen_ = false;
            }
        }
        return false;
    }

    void TypingState::reset(bool isFocusOut) {
        resetField(isFocusOut, true);
    }

    std::string TypingState::logged(const std::string& text) const {
        return host_->field().password ? std::string("(hidden)") : text;
    }

    void TypingState::resetAfterAppCommit(bool isFocusOut) {
        resetField(isFocusOut, false);
    }

    void TypingState::resetField(bool isFocusOut, bool commitPreedit) {
        // After focus out a forwarded key may reach the window that took focus.
        if (isFocusOut) {
            held_enter_pending_ = false;
            held_enter_timer_.reset();
        }
        sendHeldEnter();
        const auto  surrounding = host_->surrounding();
        const auto& text        = surrounding.text();
        size_t      textLen     = ngosen::utf8::length(text);
        realtextLen.store(textLen, std::memory_order_release);
        if (is_deleting_.load(std::memory_order_acquire)) {
            return;
        }
        tracking_modifier_tap_ = false;
        lock_word_pending_     = false;
        if (isFocusOut)
            keySinceReport_ = false;
        // GTK resets the context when Enter inserts a new line; the sentence started there still capitalizes.
        const bool keepCapitalize = shouldCapitalize_ && !isFocusOut;

        if (bambooEngine_) {
            isPrevSpace_       = false;
            isPrevHyphen_      = false;
            shouldCapitalize_  = false;
            isPrevPunctuation_ = false;
            if (realMode == ngosen::Mode::Preedit && isFocusOut && commitPreedit) {
                EngineCommitPreedit(bambooEngine_.handle());
                ngosen::UniqueCPtr<char> pulled(EnginePullCommit(bambooEngine_.handle()));
                if (pulled && (*pulled.get() != 0)) {
                    commit(pulled.get());
                    NGOSEN_INFO("Commit: " + logged(pulled.get()));
                }
            }
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
        }
        if (host_->field().frontend != "dbus")
            clearAllBuffers();
        shouldCapitalize_ = keepCapitalize;

        switch (realMode) {
            case ngosen::Mode::Preedit: {
                host_->resetPanel();
                host_->refreshPanel();
                host_->refreshPreedit();
                break;
            }
            case ngosen::Mode::Sen: {
                host_->resetPanel();
                break;
            }
            case ngosen::Mode::Emoji: {
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

    void TypingState::commitBuffer() {
        switch (realMode) {
            case ngosen::Mode::Preedit: {
                host_->resetPanel();
                if (bambooEngine_) {
                    EngineCommitPreedit(bambooEngine_.handle());
                    ngosen::UniqueCPtr<char> pulled(EnginePullCommit(bambooEngine_.handle()));
                    if (pulled && (*pulled.get() != 0))
                        commit(pulled.get());
                    ResetEngine(bambooEngine_.handle());
                }
                host_->refreshPanel();
                host_->refreshPreedit();
                break;
            }
            case ngosen::Mode::Sen: {
                if (bambooEngine_) {
                    ResetEngine(bambooEngine_.handle());
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    void TypingState::clearAllBuffers() {
        NGOSEN_DEBUG("Clear all buffers");
        if (is_deleting_.load(std::memory_order_acquire)) {
            return;
        }
        tracking_modifier_tap_ = false;
        lock_word_pending_     = false;
        oldPreBuffer_.clear();
        lastWord_.clear();
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
        if (bambooEngine_)
            ResetEngine(bambooEngine_.handle());
    }

    bool TypingState::isEmptyHistory() const {
        return !hasHistory_;
    }

} // namespace ngosen
