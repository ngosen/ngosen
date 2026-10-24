/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: retoning a selected word with the tone key typed over it.
#include "ngosen-clock.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <string>

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
            if (!std::all_of(word.begin(), word.end(), isWordChar) || (start > 0 && isWordChar(was[start - 1])) || (end < was.size() && isWordChar(was[end])))
                return {};
            // A bare letter typed over is more often code or a form value than a word.
            if (word.size() == 1 && word[0] < 0x80)
                return {};
            return charsOf(before.text(), start, word.size());
        }
    } // namespace

    void TypingState::noteRetoneKey(uint32_t sym, uint32_t states, const std::string& keyUtf8) {
        // A lagging report is fine here: only the text is compared, and the wait allows for it.
        const Field field = host_->field();
        if (!engine_->options().retoneSelection || engine_->options().outputCharset != "Unicode" || field.password || !field.surroundingText)
            return;
        // Only a key the engine keeps as typed goes to the app, which then types it over the selection.
        UniqueCPtr<char> preedit(EnginePullPreedit(bambooEngine_.handle()));
        if (!preedit || keyUtf8 != preedit.get())
            return;
        const Surrounding before = host_->surrounding();
        if (!before.isValid() || before.text().empty())
            return;
        retoneKey_    = RetoneKey{.before = before, .text = keyUtf8, .sym = sym, .states = states};
        retone_timer_ = host_->startTimer(monotonicUs() + RetoneReportUs, 0, [this](Timer&) {
            retoneKey_.reset();
            return false;
        });
    }

    void TypingState::dropRetoneKey() {
        retoneKey_.reset();
        retone_timer_.reset();
    }

    void TypingState::checkRetoneReport() {
        if (!retoneKey_)
            return;
        const Surrounding after = host_->surrounding();
        // Firefox repeats the old state before the new one.
        if (after == retoneKey_->before)
            return;
        // The replaced word is at least two bytes and the key one, so the text has to get shorter.
        if (retype_ || after.text().size() >= retoneKey_->before.text().size())
            return;
        // An app may report the deletion and the key separately.
        const std::string word = replacedWord(retoneKey_->before, after, retoneKey_->text);
        if (word.empty())
            return;
        const RetoneKey key = std::move(*retoneKey_);
        dropRetoneKey();
        const std::string toned = retoned(word, key.sym, key.states);
        if (toned.empty() || toned == word || withoutToneMark(toned) != withoutToneMark(word))
            return;
        NGOSEN_INFO("Retone: " + word + " -> " + toned);
        retype_ = Retype{.typed = key.text, .marked = toned, .atOnce = true, .retone = true};
        applyRetypeSoon(host_->translate("Tab or Esc: put back the key"));
    }

    bool TypingState::handleKeyAfterRetone(ngosen::KeyPress& keyEvent) {
        // Another tone key fixes the tone again; s, r and x also start words, so any other letter
        // means the selection was being typed over and the key goes back.
        const bool deleting = is_deleting_.load(std::memory_order_acquire);
        // The tone key twice undoes it, as typing it twice does in a word.
        if (!deleting && host_->keyText(keyEvent.sym()) == retype_->typed) {
            keyEvent.accept();
            applyRetype();
            return true;
        }
        const std::string& shown = retype_->marked;
        const std::string  toned = deleting ? std::string() : retoned(shown, keyEvent.sym(), keyEvent.states());
        if (!toned.empty() && toned != shown && withoutToneMark(toned) == withoutToneMark(shown)) {
            keyEvent.accept();
            NGOSEN_INFO("Retone: " + shown + " -> " + toned);
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
