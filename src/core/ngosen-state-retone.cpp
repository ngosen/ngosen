/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: retoning a selected word with the tone key typed over it.
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace ngosen {

    namespace {
        // Apps report a key they were handed within tens of milliseconds.
        constexpr uint64_t RetoneReportUs = 300000;

        std::string        charsOf(const std::string& text, size_t from, size_t count) {
            auto start = text.begin();
            for (size_t i = 0; i < from && start != text.end(); ++i)
                start = utf8::nextChar(start, text.end());
            auto end = start;
            for (size_t i = 0; i < count && end != text.end(); ++i)
                end = utf8::nextChar(end, text.end());
            return {start, end};
        }

        // A bare letter typed over is more often code or a form value than a word.
        bool isRetonable(const std::u32string& word) {
            return !word.empty() && std::all_of(word.begin(), word.end(), isWordChar) && (word.size() > 1 || word[0] >= 0x80);
        }

        // The word that after shows replaced by key, or nothing when after is anything else. Only the
        // text counts: Firefox and others report a selection as a bare cursor, or not at all.
        std::string replacedWord(const Surrounding& before, const Surrounding& after, const std::string& key) {
            const std::u32string was    = utf8::decode(before.text());
            const std::u32string now    = utf8::decode(after.text());
            const std::u32string typed  = utf8::decode(key);
            const size_t         cursor = after.cursor();
            if (after.anchor() != cursor || cursor < typed.size() || cursor > now.size() || now.compare(cursor - typed.size(), typed.size(), typed) != 0)
                return {};
            const size_t start = cursor - typed.size();
            const size_t tail  = now.size() - cursor;
            if (was.size() <= start + tail)
                return {};
            const size_t end = was.size() - tail;
            if (was.compare(0, start, now, 0, start) != 0 || was.compare(end, tail, now, cursor, tail) != 0)
                return {};
            const std::u32string word = was.substr(start, end - start);
            if (!isRetonable(word) || (start > 0 && isWordChar(was[start - 1])) || (end < was.size() && isWordChar(was[end])))
                return {};
            return charsOf(before.text(), start, word.size());
        }

        struct KeyInWord {
            std::string word;     // the word before the key
            std::string shown;    // the word with the key, as the field now has it
            size_t      tail = 0; // letters of the word after the key
        };

        // The word that key was typed into, when after shows before with just key added there; nothing
        // when the key went anywhere else, such as over a selection the app hides. Only a click can put
        // the key inside a word. GTK 3 reports no click, so without one or a backspace the app must have
        // reported after the previous key and the key must land away from that report's cursor.
        KeyInWord wordAroundKey(const Surrounding& before, const Surrounding& after, const std::string& key, bool clicked, bool backspaced, bool reportedSinceKey) {
            if (before.anchor() != before.cursor() || after.anchor() != after.cursor() || after.text().size() != before.text().size() + key.size())
                return {};
            if (!clicked && !backspaced && !reportedSinceKey)
                return {};
            const std::u32string was   = utf8::decode(before.text());
            const std::u32string typed = utf8::decode(key);
            if (after.cursor() < typed.size())
                return {};
            const size_t cursor = after.cursor() - typed.size();
            if (cursor > was.size())
                return {};
            if (!clicked && !backspaced && cursor == before.cursor())
                return {};
            std::u32string expected = was;
            expected.insert(cursor, typed);
            if (utf8::decode(after.text()) != expected)
                return {};
            size_t start = cursor;
            while (start > 0 && isWordChar(was[start - 1]))
                --start;
            size_t end = cursor;
            while (end < was.size() && isWordChar(was[end]))
                ++end;
            if (start == cursor || (end > cursor && !clicked) || (end - start == 1 && was[start] < 0x80))
                return {};
            return {.word = charsOf(before.text(), start, end - start), .shown = charsOf(after.text(), start, end - start + typed.size()), .tail = end - cursor};
        }
    } // namespace

    void TypingState::noteRetoneKey(uint32_t sym, uint32_t states, const std::string& keyUtf8) {
        // A lagging report is fine here: only the text is compared, and the wait allows for it.
        const Field field = host_->field();
        if (!engine_->options().retoneSelection || engine_->options().outputCharset != "Unicode" || field.password)
            return;
        // Only a key the engine keeps as typed goes to the app, which then types it over the selection.
        UniqueCPtr<char> preedit(EnginePullPreedit(bambooEngine_.handle()));
        if (!preedit || keyUtf8 != preedit.get())
            return;
        if (!field.surroundingText) {
            if (ngosen::typesOverPrimarySelection(field))
                retoneAnnounced(sym, states, keyUtf8);
            return;
        }
        const Surrounding before = host_->surrounding();
        if (!before.isValid() || before.text().empty())
            return;
        retoneKey_    = RetoneKey{.before           = before,
                                  .text             = keyUtf8,
                                  .sym              = sym,
                                  .states           = states,
                                  .afterClick       = clickBeforeKey_,
                                  .afterBackspace   = backspaceBeforeKey_,
                                  .reportedSinceKey = !keySinceReport_};
        retone_timer_ = host_->startTimer(monotonicUs() + RetoneReportUs, 0, [this](Timer&) {
            retoneKey_.reset();
            return false;
        });
    }

    void TypingState::retoneAnnounced(uint32_t sym, uint32_t states, const std::string& keyUtf8) {
        // The field cannot show which word the key replaced; the X server says what was selected.
        const std::optional<std::string> word = std::exchange(selectedBeforeKey_, std::nullopt);
        if (word && isRetonable(utf8::decode(*word)))
            offerRetone(*word, keyUtf8, 0, sym, states, keyUtf8);
    }

    void TypingState::forgetSelection() {
        selectedBeforeKey_.reset();
        host_->takeSelection();
    }

    void TypingState::offerRetone(const std::string& word, std::string typed, size_t tail, uint32_t sym, uint32_t states, const std::string& key) {
        const std::string toned = retoned(word, sym, states);
        if (toned.empty() || toned == word || withoutToneMark(toned) != withoutToneMark(word))
            return;
        NGOSEN_INFO("Retone: " + logged(word) + " -> " + logged(toned));
        Retype retype{.typed = std::move(typed), .marked = toned, .atOnce = true, .retone = true, .inWord = tail > 0, .key = key};
        if (tail > 0) {
            retoneAtWordEnd(tail, std::move(retype));
            return;
        }
        retype_ = std::move(retype);
        applyRetypeSoon(host_->translate("Tab or Esc: put back the key"));
    }

    void TypingState::dropRetoneKey() {
        retoneKey_.reset();
        retoneMove_.reset();
        retone_timer_.reset();
    }

    void TypingState::retoneAtWordEnd(size_t tail, Retype retype) {
        const Surrounding at = host_->surrounding();
        retoneMove_          = RetoneMove{.text = at.text(), .cursor = at.cursor() + tail, .retype = std::move(retype)};
        for (size_t i = 0; i < tail; ++i)
            forwardTap(EditKey::Right);
        // A field that never shows the cursor there keeps the key as typed.
        retone_timer_ = host_->startTimer(monotonicUs() + RetoneReportUs, 0, [this](Timer&) {
            NGOSEN_DEBUG("Retone: the cursor never reached the word's end");
            retoneMove_.reset();
            return false;
        });
    }

    void TypingState::checkRetoneReport() {
        if (retoneMove_) {
            const Surrounding at = host_->surrounding();
            if (at.text() != retoneMove_->text || at.cursor() != retoneMove_->cursor || at.anchor() != at.cursor())
                return;
            retype_ = std::move(retoneMove_->retype);
            dropRetoneKey();
            applyRetypeSoon(host_->translate("Tab or Esc: put back the key"));
            return;
        }
        if (!retoneKey_)
            return;
        const Surrounding after = host_->surrounding();
        // Firefox repeats the old state before the new one.
        if (after == retoneKey_->before)
            return;
        if (retype_)
            return;
        // The replaced word is at least two bytes and the key one, so the text has to get shorter.
        // An app may report the deletion and the key separately.
        std::string word  = after.text().size() < retoneKey_->before.text().size() ? replacedWord(retoneKey_->before, after, retoneKey_->text) : std::string();
        std::string typed = retoneKey_->text;
        size_t      tail  = 0;
        // A click or backspace leaves nothing selected, so the key lands in the word.
        if (word.empty()) {
            const KeyInWord found = wordAroundKey(retoneKey_->before, after, retoneKey_->text, retoneKey_->afterClick, retoneKey_->afterBackspace, retoneKey_->reportedSinceKey);
            word                  = found.word;
            typed                 = found.shown;
            tail                  = found.tail;
        }
        if (word.empty())
            return;
        const RetoneKey key = std::move(*retoneKey_);
        dropRetoneKey();
        offerRetone(word, typed, tail, key.sym, key.states, key.text);
    }

    bool TypingState::handleKeyAfterRetone(ngosen::KeyPress& keyEvent) {
        // Another tone key fixes the tone again; s, r and x also start words, so any other letter
        // means the selection was being typed over and the key goes back.
        const bool deleting = is_deleting_.load(std::memory_order_acquire);
        // The tone key twice undoes it, as typing it twice does in a word.
        const std::string key = host_->keyText(keyEvent.sym());
        if (!deleting && !key.empty() && key == retype_->key) {
            keyEvent.accept();
            applyRetype();
            return true;
        }
        const std::string& shown = retype_->marked;
        const std::string  toned = deleting ? std::string() : retoned(shown, keyEvent.sym(), keyEvent.states());
        if (!toned.empty() && toned != shown && withoutToneMark(toned) == withoutToneMark(shown)) {
            keyEvent.accept();
            NGOSEN_INFO("Retone: " + logged(shown) + " -> " + logged(toned));
            replaceRetyped(shown, toned);
            retype_->marked = toned;
            return true;
        }
        reopenTypedKey();
        return false;
    }

    std::string TypingState::retoned(const std::string& word, uint32_t sym, uint32_t states) const {
        // Its own engine, so the one typing in the field keeps the key whatever the user does next.
        CGoObject engine(newBambooEngine());
        setOption(engine.handle());
        EngineRebuildFromText(engine.handle(), word.c_str());
        EngineProcessKeyEvent(engine.handle(), sym, states);
        UniqueCPtr<char> early(EnginePullCommit(engine.handle()));
        if (early && *early.get() != 0)
            return {};
        // Ending the word applies the user's restore of invalid words, as typing it would.
        EngineProcessKeyEvent(engine.handle(), key::space, 0);
        UniqueCPtr<char> commit(EnginePullCommit(engine.handle()));
        std::string      toned = commit ? commit.get() : "";
        if (toned.empty() || toned.back() != ' ')
            return {};
        toned.pop_back();
        return toned;
    }

} // namespace ngosen
