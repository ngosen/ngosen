/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fake-app.h"

#include "fake-engine.h"
#include "ngosen-clock.h"
#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-state.h"
#include "ngosen-utf8.h"

namespace ngosen::test {

    namespace {

        std::string encode(const std::u32string& text) {
            std::string out;
            for (char32_t c : text) {
                if (c < 0x80) {
                    out += static_cast<char>(c);
                } else if (c < 0x800) {
                    out += static_cast<char>(0xc0 | (c >> 6));
                    out += static_cast<char>(0x80 | (c & 0x3f));
                } else if (c < 0x10000) {
                    out += static_cast<char>(0xe0 | (c >> 12));
                    out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
                    out += static_cast<char>(0x80 | (c & 0x3f));
                } else {
                    out += static_cast<char>(0xf0 | (c >> 18));
                    out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
                    out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
                    out += static_cast<char>(0x80 | (c & 0x3f));
                }
            }
            return out;
        }

    } // namespace

    std::string telexOutput(const std::vector<uint32_t>& keys) {
        realMode = Mode::Preedit;
        realtextLen.store(0);
        FakeLoop      loop;
        FakeResources resources;
        Field         field;
        field.frontend    = "wayland";
        field.preedit     = true;
        auto        owned = std::make_unique<FakeHost>(loop, field);
        FakeHost*   host  = owned.get();
        TypingState state(&resources, std::move(owned));
        std::string text;
        size_t      seenCommits = 0;
        for (uint32_t sym : keys) {
            FakeKey down(sym);
            state.keyEvent(down);
            for (; seenCommits < host->commits().size(); ++seenCommits)
                text += host->commits()[seenCommits];
            if (!down.accepted()) {
                if (sym == key::BackSpace)
                    utf8::eraseLastCodepoint(text);
                else
                    text += host->keyText(down.appSym());
            }
            FakeKey up(sym, true);
            state.keyEvent(up);
        }
        return text + host->preedit();
    }

    std::string typeInApp(const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs) {
        realMode = Mode::Sen;
        realtextLen.store(0);
        FakeLoop      loop;
        FakeResources resources;
        auto          owned = std::make_unique<FakeApp>(loop, field, quirks);
        FakeApp*      app   = owned.get();
        TypingState   state(&resources, std::move(owned));
        app->attach(&state);
        for (size_t i = 0; i < keys.size(); ++i) {
            app->type(keys[i]);
            loop.pump(gapsMs[i]);
        }
        loop.pump(300);
        return app->text();
    }

    void FakeApp::type(uint32_t sym) {
        FakeKey down(sym);
        state_->keyEvent(down);
        if (!down.accepted()) {
            if (sym == key::BackSpace)
                later(quirks_.lagMs, [this] { eraseBefore(1); });
            else if (const auto text = keyText(down.appSym()); !text.empty())
                later(quirks_.lagMs, [this, text] { insert(utf8::decode(text)); });
        }
        // What LotusEngine::keyEvent does after the typing logic: trust a report with the cursor at the end.
        const Surrounding s = surrounding();
        if (s.isValid() && utf8::length(s.text()) == s.cursor())
            realtextLen.store(s.cursor(), std::memory_order_release);

        FakeKey up(sym, true);
        state_->keyEvent(up);
    }

    std::string FakeApp::text() const {
        return encode(text_);
    }

    void FakeApp::commitText(const std::string& text) {
        // fcitx tells the typing logic about every commit as it is sent; LotusEngine watches for it.
        state_->noteCommit(text);
        later(quirks_.lagMs, [this, text] { insert(utf8::decode(text)); });
    }

    void FakeApp::forwardKey(EditKey key, bool release) {
        if (key != EditKey::BackSpace || release)
            return;
        later(quirks_.lagMs, [this] { eraseBefore(1); });
    }

    void FakeApp::deleteSurrounding(int offset, unsigned int size) {
        later(quirks_.lagMs, [this, offset, size] { eraseAround(offset, size); });
    }

    void FakeApp::eraseAround(int offset, unsigned int size) {
        const long from = static_cast<long>(cursor_) + offset;
        if (from < 0 || static_cast<size_t>(from) + size > text_.size())
            return;
        previousText_   = text_;
        previousCursor_ = cursor_;
        text_.erase(static_cast<size_t>(from), size);
        if (cursor_ > static_cast<size_t>(from))
            cursor_ = cursor_ >= static_cast<size_t>(from) + size ? cursor_ - size : static_cast<size_t>(from);
        changed();
    }

    Surrounding FakeApp::surrounding() const {
        if (!field_.surroundingText)
            return {};
        if (quirks_.staleReport)
            return Surrounding(encode(previousText_), static_cast<unsigned int>(previousCursor_), static_cast<unsigned int>(previousCursor_));
        return Surrounding(encode(text_), static_cast<unsigned int>(cursor_), static_cast<unsigned int>(cursor_));
    }

    std::string FakeApp::keyText(uint32_t sym) const {
        if (sym < 0x20 || sym > 0xff || (sym >= 0x7f && sym < 0xa0))
            return {};
        return encode(std::u32string(1, static_cast<char32_t>(sym)));
    }

    void FakeApp::insert(const std::u32string& text) {
        previousText_   = text_;
        previousCursor_ = cursor_;
        text_.insert(cursor_, text);
        cursor_ += text.size();
        changed();
    }

    void FakeApp::eraseBefore(size_t count) {
        count = count < cursor_ ? count : cursor_;
        if (count == 0)
            return;
        previousText_   = text_;
        previousCursor_ = cursor_;
        text_.erase(cursor_ - count, count);
        cursor_ -= count;
        changed();
    }

    void FakeApp::changed() {
        if (field_.surroundingText && quirks_.reports)
            later(quirks_.reportDelayMs, [this] { state_->surroundingUpdated(); });
    }

    void FakeApp::later(int ms, std::function<void()> action) {
        timers_.push_back(loop_.add(monotonicUs() + static_cast<uint64_t>(ms) * 1000, [action = std::move(action)](Timer&) {
            action();
            return false;
        }));
    }

} // namespace ngosen::test
