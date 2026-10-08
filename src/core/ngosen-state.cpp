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
#include "ngosen-utf8.h"

#include <cstddef>
#include <string>
#include <thread>

namespace ngosen {

    // Chromium asks the input method about each key before it handles the key, so our Shift release
    // coming back does not mean the Left presses before it have moved the selection yet.
    constexpr uint64_t XTestSelectSettleUs = 50000;

    TypingState::TypingState(ngosen::EngineResources* engine, std::unique_ptr<ngosen::Host> host) : engine_(engine), host_(std::move(host)) {
        setEngine();
    }

    void TypingState::setEngine() {
        bambooEngine_.reset();

        if (engine_->options().inputMethod == "Custom") {
            const auto         keymaps = engine_->customKeymap();
            std::vector<char*> charArray;
            charArray.reserve((keymaps.size() * 2) + 1);
            for (const auto& keymap : keymaps) {
                charArray.push_back(const_cast<char*>(keymap.key.data()));   //NOLINT
                charArray.push_back(const_cast<char*>(keymap.value.data())); //NOLINT
            }
            charArray.push_back(nullptr);
            bambooEngine_.reset(NewCustomEngine(charArray.data(), engine_->dictionary(), engine_->macroTable()));
        } else {
            bambooEngine_.reset(NewEngine(engine_->options().inputMethod.data(), engine_->dictionary(), engine_->macroTable()));
        }
        setOption();
        resetMacroSkip();
    }

    void TypingState::setOption() {
        if (!bambooEngine_)
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

        EngineSetOption(bambooEngine_.handle(), &option);
    }

    void TypingState::handlePreeditMode(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (EngineProcessKeyEvent(bambooEngine_.handle(), currentSym, keyEvent.states()) != 0U)
            keyEvent.accept();
        if (auto commit = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()))) {
            if (commit && (*commit.get() != 0)) {
                NGOSEN_INFO("Commit: " + std::string(commit.get()));
                host_->commitText(commit.get());
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

    void TypingState::handleUinputMode(ngosen::KeyPress& keyEvent, uint32_t currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.passToApp();
            return;
        }

        if (ngosen::key::isBackspace(currentSym) || currentSym == ngosen::key::Return) {
            if (ngosen::key::isBackspace(currentSym)) {
                hasHistory_ = true;
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

        bool processed = EngineProcessKeyEvent(bambooEngine_.handle(), currentSym, keyEvent.states()) != 0U;

        auto commitF = ngosen::UniqueCPtr<char>(EnginePullCommit(bambooEngine_.handle()));
        if (commitF && (*commitF.get() != 0)) {
            std::string commitStr = commitF.get();
            std::string deletedPart;
            std::string addedPart;
            ngosen::utf8::compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

            if (!deletedPart.empty()) {
                performReplacement(deletedPart, addedPart);
                keyEvent.accept();
            } else {
                bool wasAutoCapitalized = (currentSym != keyEvent.sym());
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
                    keyEvent.accept();
                } else {
                    keyEvent.passToApp();
                }
            }

            hasHistory_ = false;
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();

            return;
        }

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
                        host_->commitText(addedPart);
                        NGOSEN_INFO("Commit: " + addedPart);
                        keyEvent.accept();
                        isCommit = true;
                    }
                }
                if (!isCommit) {
                    keyEvent.passToApp();
                }
            } else {
                if (!canSendBackspaces()) {
                    NGOSEN_ERROR("Cannot send backspaces here, commit rawkey");
                    std::string rawKey = keyEvent.name();
                    if (!rawKey.empty()) {
                        host_->commitText(rawKey);
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

    void TypingState::keyEvent(ngosen::KeyPress& keyEvent) {
        if (!bambooEngine_)
            return;
        if (overtype_pending_ && overtype_via_xtest_ && keyEvent.sym() == ngosen::key::Shift_R) {
            keyEvent.passToApp();
            if (keyEvent.isRelease() && !overtype_shift_released_ && overtype_timer_) {
                overtype_shift_released_ = true;
                overtype_timer_->rearm(ngosen::monotonicUs() + XTestSelectSettleUs);
            }
            return;
        }
        if (realMode == ngosen::Mode::Preedit) {
            if (keyEvent.isBareShift())
                return;
        } else {
            if (keyEvent.isModifier()) {
                handleModifierTap(keyEvent);
                return;
            }
            cancelModifierTap();
        }
        if (keyEvent.isRelease())
            return;
        lastInputAtUs_ = ngosen::monotonicUs();
        // An XIM client sometimes sends a key we let through back to us instead of typing it. Let it
        // through again rather than type it twice. Our own XTEST backspaces share one time, so they
        // are exempt.
        if (!is_deleting_.load(std::memory_order_acquire) && keyEvent.time() != 0 && keyEvent.time() == lastPressTime_ && keyEvent.code() == lastPressCode_) {
            NGOSEN_INFO("App sent a key back: " + keyEvent.name());
            keyEvent.passToApp();
            return;
        }
        lastPressCode_ = keyEvent.code();
        lastPressTime_ = keyEvent.time();
        if (const uint32_t rawSym = keyEvent.sym(); overtype_pending_ && (rawSym == ngosen::key::Left || rawSym == ngosen::key::Shift_L || rawSym == ngosen::key::Shift_R)) {
            // Our own Shift+Left selection: it must reach the app and must not be treated as the user
            // moving the cursor (that would discard the pending commit).
            keyEvent.passToApp();
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

        // A key that beats the deferred commit must not land before it.
        flushDeferredCommit();
        if (g_mouse_clicked.load(std::memory_order_acquire) && !is_deleting_.load(std::memory_order_acquire)) {
            g_mouse_clicked.store(false, std::memory_order_release);
            clearAllBuffers();
        }
        uint32_t currentSym = keyEvent.sym();
        if (engine_->options().autoCapitalizeAfterPunctuation && realMode != ngosen::Mode::Off) {
            // Ignore auto-capitalize side-effects if we're processing automated replacement backspaces
            bool isAutomatedBackspace = is_deleting_.load(std::memory_order_acquire) && ngosen::key::isBackspace(currentSym);

            if (!isAutomatedBackspace) {
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
                    default:
                        if (currentSym != ngosen::key::space) {
                            isPrevPunctuation_ = false;
                        }
                        break;
                }
            }
        }

        if (is_deleting_.load(std::memory_order_acquire) && surr_wait_timer_only_ && ngosen::key::isBackspace(currentSym) && host_->field().frontend == "xim" &&
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
        if (is_deleting_.load(std::memory_order_acquire) && ngosen::key::isBackspace(currentSym) && ngosen::forwardsBackspaces(host_->field()) && surr_wait_pending_) {
            // Forwarded backspaces never come back, so this one is the user's: finish the replacement
            // first, then handle it normally.
            finishReplacement("backspace arrived", false);
        }
        if (is_deleting_.load(std::memory_order_acquire)) {
            if (ngosen::key::isBackspace(currentSym)) {
                if (realtextLen.load(std::memory_order_acquire) > 0)
                    realtextLen.fetch_sub(1, std::memory_order_acq_rel);
                if (handleUInputKeyPress(keyEvent, currentSym, 4)) {
                    return;
                }
            } else {
                std::string keyUtf8Check = host_->keyText(currentSym);
                if (!keyUtf8Check.empty() && buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                    NGOSEN_WARN("Typing so fast, add key to queue");
                    buffered_keys_.push_back({.sym = currentSym, .state = keyEvent.states()});
                }
                keyEvent.accept();
            }
            return;
        }

        if (engine_->options().doubleSpaceToPeriod && realMode != ngosen::Mode::Off) {
            bool isSpaceKey = (currentSym == ngosen::key::space || currentSym == ngosen::key::KP_Space);
            if (isSpaceKey && !keyEvent.hasModifier()) {
                if (isPrevSpace_) {
                    keyEvent.accept();
                    handleDoubleSpaceReplacement();
                    isPrevSpace_ = false;
                    return;
                }
                isPrevSpace_ = true;
            } else {
                isPrevSpace_ = false;
            }
        }

        if (engine_->options().doubleHyphenToEmDash && realMode != ngosen::Mode::Off) {
            bool isHyphenKey = (currentSym == ngosen::key::minus || currentSym == ngosen::key::KP_Subtract);
            if (isHyphenKey && !keyEvent.hasModifier()) {
                if (isPrevHyphen_) {
                    keyEvent.accept();
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
            case ngosen::Mode::Sen: {
                handleUinputMode(keyEvent, currentSym);
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
                if (engine_->options().enableMacroInOffMode && engine_->options().enableMacro) {
                    handleOffModeMacro(keyEvent, currentSym);
                }
                break;
            }
        }
        reEnableMacroAfterWordEnd();
    }

    void TypingState::reset(bool isFocusOut) {
        const auto  surrounding = host_->surrounding();
        const auto& text        = surrounding.text();
        size_t      textLen     = ngosen::utf8::length(text);
        realtextLen.store(textLen, std::memory_order_release);
        if (is_deleting_.load(std::memory_order_acquire)) {
            return;
        }
        resetMacroSkip();

        if (bambooEngine_) {
            isPrevSpace_       = false;
            isPrevHyphen_      = false;
            shouldCapitalize_  = false;
            isPrevPunctuation_ = false;
            if (realMode == ngosen::Mode::Preedit && isFocusOut) {
                EngineCommitPreedit(bambooEngine_.handle());
                ngosen::UniqueCPtr<char> commit(EnginePullCommit(bambooEngine_.handle()));
                if (commit && (*commit.get() != 0)) {
                    host_->commitText(commit.get());
                    NGOSEN_INFO("Commit: " + std::string(commit.get()));
                }
            }
            ResetEngine(bambooEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
        }
        if (host_->field().frontend != "dbus")
            clearAllBuffers();

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
                    ngosen::UniqueCPtr<char> commit(EnginePullCommit(bambooEngine_.handle()));
                    if (commit && (*commit.get() != 0))
                        host_->commitText(commit.get());
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
        if (bambooEngine_)
            ResetEngine(bambooEngine_.handle());
    }

    bool TypingState::isEmptyHistory() const {
        return !hasHistory_;
    }

} // namespace ngosen
