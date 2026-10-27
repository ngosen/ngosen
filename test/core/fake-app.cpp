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

#include <algorithm>

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
        FakeResources resources;
        return telexOutput(resources, keys);
    }

    std::string telexOutput(FakeResources& resources, const std::vector<uint32_t>& keys) {
        realMode = Mode::Preedit;
        realtextLen.store(0);
        FakeLoop loop;
        Field    field;
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

    std::string typeInApp(const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs) {
        FakeResources resources;
        return typeInApp(resources, field, quirks, keys, gapsMs, slowestCommitMs);
    }

    std::string typeInApp(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs) {
        realMode = Mode::Sen;
        realtextLen.store(0);
        FakeLoop    loop;
        auto        owned = std::make_unique<FakeApp>(loop, field, quirks);
        FakeApp*    app   = owned.get();
        TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (size_t i = 0; i < keys.size(); ++i) {
            app->type(keys[i]);
            loop.pump(gapsMs[i]);
        }
        loop.pump(300);
        if (slowestCommitMs != nullptr)
            *slowestCommitMs = app->slowestCommitMs();
        return app->text();
    }

    void FakeApp::type(uint32_t sym) {
        const auto time = static_cast<uint32_t>(monotonicUs() / 1000);
        FakeKey    down(sym, false, 0, time);
        state_->keyEvent(down);
        if (!down.accepted() && quirks_.resendsKeysWithoutText && keyText(sym).empty()) {
            FakeKey again(sym, false, modifier::Repeat, time);
            state_->keyEvent(again);
        }
        if (!down.accepted()) {
            if (sym == key::BackSpace) {
                lastBackSpaceAt_ = time;
                later(quirks_.lagMs, [this] { eraseBefore(1); });
            } else if (const auto text = keyText(down.appSym()); !text.empty() && static_cast<unsigned char>(text[0]) >= 0x20 && text[0] != '\x7f')
                later(quirks_.lagMs, [this, text] { insert(utf8::decode(text)); });
        }

        FakeKey up(sym, true);
        state_->keyEvent(up);
    }

    void FakeApp::moveCursor(size_t to) {
        previousText_   = text_;
        previousCursor_ = cursor_;
        cursor_         = to < text_.size() ? to : text_.size();
        anchor_         = cursor_;
        if (!quirks_.hidesCursorMoves)
            changed();
    }

    void FakeApp::select(size_t anchor, size_t cursor) {
        previousText_   = text_;
        previousCursor_ = cursor_;
        anchor_         = anchor;
        cursor_         = cursor;
        if (!quirks_.hidesCursorMoves)
            changed();
    }

    bool FakeApp::eraseSelection() {
        if (anchor_ == cursor_)
            return false;
        const size_t from = std::min(anchor_, cursor_);
        text_.erase(from, std::max(anchor_, cursor_) - from);
        cursor_ = anchor_ = from;
        return true;
    }

    std::string FakeApp::text() const {
        return encode(text_);
    }

    void FakeApp::commitText(const std::string& text) {
        if (forwardedAt_ > 0) {
            slowestCommitMs_ = std::max(slowestCommitMs_, static_cast<int>((monotonicUs() - forwardedAt_) / 1000));
            forwardedAt_     = 0;
        }
        if ((quirks_.dropsCommitsInReport && inReport_) || !focused_ || redraws_ > 0)
            return;
        later(quirks_.lagMs + quirks_.commitLagMs, [this, text] { insert(utf8::decode(text)); });
    }

    void FakeApp::forwardKey(EditKey key, bool release, uint32_t time) {
        if (key == EditKey::Right && !release && !quirks_.dropsForwardedKeys) {
            later(quirks_.lagMs, [this] { moveCursor(cursor_ + 1); });
            return;
        }
        if (key != EditKey::BackSpace || release || quirks_.dropsForwardedKeys)
            return;
        if (quirks_.dropsOldKeyPresses) {
            if (lastBackSpaceAt_ != 0 && static_cast<int32_t>(time - lastBackSpaceAt_) <= 0)
                return;
            lastBackSpaceAt_ = time;
        }
        if (forwardedAt_ == 0)
            forwardedAt_ = monotonicUs();
        if (quirks_.keyStepMs > 0) {
            later(quirks_.keyStepMs * ++queuedKeys_, [this] {
                --queuedKeys_;
                eraseBefore(1);
            });
        } else if (quirks_.handsBackKeys)
            later(quirks_.lagMs, [this] { type(key::BackSpace); });
        else
            later(quirks_.lagMs, [this] { eraseBefore(1); });
    }

    bool FakeApp::pressSystemKeys(int count) {
        if (!quirks_.takesXTest || count <= 0)
            return false;
        later(quirks_.lagMs, [this, count] {
            // The X server stamps the burst with one time, and fcitx5's XIM frontend then marks every press after the first as a repeat.
            const auto time = static_cast<uint32_t>(monotonicUs() / 1000);
            for (int i = 0; i < count; ++i) {
                FakeKey down(key::BackSpace, false, i == 0 ? 0 : modifier::Repeat, time);
                state_->keyEvent(down);
                if (!down.accepted())
                    eraseBefore(1);
                FakeKey up(key::BackSpace, true, 0, time);
                state_->keyEvent(up);
            }
        });
        return true;
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
        anchor_         = cursor_;
        text_.erase(static_cast<size_t>(from), size);
        if (cursor_ > static_cast<size_t>(from))
            cursor_ = cursor_ >= static_cast<size_t>(from) + size ? cursor_ - size : static_cast<size_t>(from);
        changed();
    }

    Surrounding FakeApp::surrounding() const {
        if (!field().surroundingText)
            return {};
        if (quirks_.staleReport)
            return Surrounding(encode(previousText_), static_cast<unsigned int>(previousCursor_), static_cast<unsigned int>(previousCursor_));
        if (quirks_.hidesCursorMoves)
            return Surrounding(encode(text_), static_cast<unsigned int>(shownCursor_), static_cast<unsigned int>(shownAnchor_));
        if (quirks_.hidesSelectionEnd) {
            const auto start = static_cast<unsigned int>(std::min(anchor_, cursor_));
            return Surrounding(encode(text_), start, start);
        }
        return Surrounding(encode(text_), static_cast<unsigned int>(cursor_), static_cast<unsigned int>(anchor_));
    }

    std::string FakeApp::keyText(uint32_t sym) const {
        // fcitx gives these keys their control characters.
        switch (sym) {
            case key::BackSpace: return "\b";
            case key::Tab: return "\t";
            case key::Return: return "\r";
            case key::Escape: return "\x1b";
            case key::Delete: return "\x7f";
            default: break;
        }
        if (sym < 0x20 || sym > 0xff || (sym >= 0x7f && sym < 0xa0))
            return {};
        return encode(std::u32string(1, static_cast<char32_t>(sym)));
    }

    void FakeApp::insert(const std::u32string& text) {
        if (quirks_.erasesOnDel && !text.empty() && text[0] == U'\x7f') {
            const size_t dels = text.find_first_not_of(U'\x7f');
            eraseBefore(dels == std::u32string::npos ? text.size() : dels);
            if (dels != std::u32string::npos)
                insert(text.substr(dels));
            return;
        }
        previousText_   = text_;
        previousCursor_ = cursor_;
        eraseSelection();
        text_.insert(cursor_, text);
        cursor_ += text.size();
        anchor_ = cursor_;
        changed();
        if (!quirks_.redrawsOnlyOnDeletion)
            redrawAfterEdit();
    }

    void FakeApp::redrawAfterEdit() {
        if (quirks_.redrawAfterEditMs <= 0)
            return;
        ++redraws_;
        later(quirks_.redrawAfterEditMs, [this] {
            state_->deactivate(true);
            state_->activate(Mode::Sen, true);
            --redraws_;
            changed();
        });
    }

    void FakeApp::eraseBefore(size_t count) {
        if (anchor_ != cursor_) {
            previousText_   = text_;
            previousCursor_ = cursor_;
            eraseSelection();
            changed();
            if (--count == 0)
                return;
        }
        count = count < cursor_ ? count : cursor_;
        if (count == 0)
            return;
        previousText_   = text_;
        previousCursor_ = cursor_;
        text_.erase(cursor_ - count, count);
        cursor_ -= count;
        anchor_ = cursor_;
        if (text_.empty() && quirks_.refocusWhenEmptiedMs > 0) {
            focused_ = false;
            state_->deactivate(true);
            later(quirks_.refocusWhenEmptiedMs, [this] {
                focused_ = true;
                state_->activate(Mode::Sen, true);
                changed();
            });
            return;
        }
        changed();
        redrawAfterEdit();
    }

    void FakeApp::changed() {
        shownCursor_ = cursor_;
        shownAnchor_ = anchor_;
        if (field_.surroundingText && quirks_.reports)
            later(quirks_.reportDelayMs, [this] {
                inReport_ = true;
                state_->surroundingUpdated();
                inReport_ = false;
            });
    }

    void FakeApp::later(int ms, std::function<void()> action) {
        timers_.push_back(loop_.add(monotonicUs() + static_cast<uint64_t>(ms) * 1000, [action = std::move(action)](Timer&) {
            action();
            return false;
        }));
    }

} // namespace ngosen::test
