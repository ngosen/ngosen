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

    namespace {

        Typed typeKeys(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs) {
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
            return {.text = app->text(), .sent = app->sent()};
        }
    } // namespace

    std::string typeInApp(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs) {
        return typeKeys(resources, field, quirks, keys, gapsMs, slowestCommitMs).text;
    }

    Typed typeInAppTraced(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs) {
        return typeKeys(resources, field, quirks, keys, gapsMs, nullptr);
    }

    std::vector<std::string> pathMismatches(const Path& sent, const Path& expected) {
        std::vector<std::string> out;
        const auto               expect = [&out](const char* what, int want, int got) {
            if (want >= 0 && want != got)
                out.push_back(std::string(what) + ": want " + std::to_string(want) + ", got " + std::to_string(got));
        };
        expect("surrounding deletions", expected.surroundingDeletes, sent.surroundingDeletes);
        expect("forwarded backspaces", expected.forwardedBackSpaces, sent.forwardedBackSpaces);
        expect("system key presses", expected.systemKeyPresses, sent.systemKeyPresses);
        expect("preedits", expected.preedits, sent.preedits);
        return out;
    }

    void FakeApp::type(uint32_t sym) {
        const auto     time  = static_cast<uint32_t>(monotonicUs() / 1000);
        const uint32_t state = (sym & WithShift) != 0U ? modifier::Shift : 0U;
        sym &= ~WithShift;
        FakeKey down(sym, false, state, time);
        state_->keyEvent(down);
        if (!down.accepted() && quirks_.keepsLastEditPerTurn)
            endTurn();
        if (!down.accepted() && quirks_.resendsKeysWithoutText && keyText(sym).empty()) {
            FakeKey again(sym, false, modifier::Repeat, time);
            state_->keyEvent(again);
        }
        if (!down.accepted()) {
            if (sym == key::Return && quirks_.showsEnter)
                later(quirks_.lagMs, [this, soft = state == modifier::Shift] { insert(soft ? U"\u2028" : U"\n", true); });
            else if ((sym == key::Tab || sym == key::ISO_Left_Tab) && quirks_.showsTab)
                later(quirks_.lagMs, [this, back = sym == key::ISO_Left_Tab || (state & modifier::Shift) != 0U] { insert(back ? U"⇤" : U"⇥", true); });
            else if (sym == key::BackSpace) {
                lastBackSpaceAt_ = time;
                later(quirks_.lagMs, [this] { eraseBefore(1); });
            } else if (const auto text = keyText(down.appSym()); !text.empty() && static_cast<unsigned char>(text[0]) >= 0x20 && text[0] != '\x7f')
                later(quirks_.lagMs, [this, text] { insert(utf8::decode(text), true); });
        }

        FakeKey up(sym, true, state);
        state_->keyEvent(up);
    }

    void FakeApp::moveCursor(size_t to) {
        previousText_   = text_;
        previousCursor_ = cursor_;
        cursor_         = to < text_.size() ? to : text_.size();
        anchor_         = cursor_;
        announced_.reset();
        if (!quirks_.hidesCursorMoves)
            changed();
    }

    void FakeApp::select(size_t anchor, size_t cursor) {
        previousText_   = text_;
        previousCursor_ = cursor_;
        anchor_         = anchor;
        cursor_         = cursor;
        if (quirks_.announcesSelection && anchor != cursor)
            announced_ = encode(text_.substr(std::min(anchor, cursor), anchor > cursor ? anchor - cursor : cursor - anchor));
        if (!quirks_.hidesCursorMoves)
            changed();
    }

    void FakeApp::suggest(const std::string& tail, bool selected) {
        previousText_    = text_;
        previousCursor_  = cursor_;
        const auto added = utf8::decode(tail);
        text_.insert(cursor_, added);
        anchor_ = cursor_;
        if (selected)
            cursor_ += added.size();
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
        // Focus and redraws are checked again when the app gets to the commit, which can be after a
        // deletion sent just before it has emptied the field.
        if ((quirks_.dropsCommitsInReport && inReport_) || !focused_ || redraws_ > 0)
            return;
        if (quirks_.keepsLastEditPerTurn) {
            turnCommit_ = text;
            startTurn();
            return;
        }
        later(quirks_.lagMs + quirks_.commitLagMs, [this, text] {
            if (focused_ && redraws_ == 0)
                insert(utf8::decode(text));
        });
    }

    void FakeApp::forwardKey(EditKey key, bool release, uint32_t time) {
        if ((key == EditKey::Tab || key == EditKey::ShiftTab) && !release && quirks_.showsTab && !quirks_.dropsForwardedKeys) {
            later(quirks_.lagMs, [this, key] { insert(key == EditKey::ShiftTab ? U"⇤" : U"⇥", true); });
            return;
        }
        if ((key == EditKey::Return || key == EditKey::ShiftReturn) && !release && quirks_.showsEnter && !quirks_.dropsForwardedKeys) {
            later(quirks_.lagMs, [this, key] { insert(key == EditKey::ShiftReturn ? U"\u2028" : U"\n", true); });
            return;
        }
        if (key == EditKey::Right && !release && !quirks_.dropsForwardedKeys) {
            later(quirks_.lagMs, [this] { moveCursor(cursor_ + 1); });
            return;
        }
        if (key != EditKey::BackSpace || release)
            return;
        ++sent_.forwardedBackSpaces;
        if (quirks_.dropsForwardedKeys)
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
            later(quirks_.lagMs + quirks_.backspaceLagMs, [this] { eraseBefore(1); });
    }

    bool FakeApp::pressSystemKeys(int count) {
        if (!quirks_.takesXTest || count <= 0)
            return false;
        sent_.systemKeyPresses += count;
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
        ++sent_.surroundingDeletes;
        // A module whose copy of the text is empty finds every deletion out of range.
        if (quirks_.ignoresSurroundingDel || quirks_.reportsEmptyText)
            return;
        if (quirks_.keepsLastEditPerTurn && offset == -static_cast<int>(size)) {
            turnDeleteBytes_ = reportedBytesBefore(size);
            startTurn();
            return;
        }
        if (quirks_.convertsDeleteOnReportedCopy && offset == -static_cast<int>(size)) {
            const size_t bytes = reportedBytesBefore(size);
            later(quirks_.lagMs, [this, bytes] {
                const unsigned int chars = charsForBytesBefore(bytes);
                eraseAround(-static_cast<int>(chars), chars);
            });
            return;
        }
        later(quirks_.lagMs, [this, offset, size] { eraseAround(offset, size); });
    }

    size_t FakeApp::reportedBytesBefore(unsigned int size) const {
        const size_t from = reportedCursor_ >= size ? reportedCursor_ - size : 0;
        return encode(reportedText_.substr(from, reportedCursor_ - from)).size();
    }

    // The app finds where the deletion starts by decoding the bytes before it, as Qt's
    // indexFromWayland does; a cut inside a character leaves that character in place.
    unsigned int FakeApp::charsForBytesBefore(size_t bytes) const {
        const std::string before = encode(text_.substr(0, cursor_));
        const size_t      kept   = bytes < before.size() ? before.size() - bytes : 0;
        size_t            start  = 0;
        for (size_t i = 0; i < kept; ++i)
            if ((static_cast<unsigned char>(before[i]) & 0xc0) != 0x80)
                ++start;
        return static_cast<unsigned int>(cursor_ - start);
    }

    void FakeApp::eraseAround(int offset, unsigned int size) {
        const long from = static_cast<long>(cursor_) + offset;
        if (from < 0 || static_cast<size_t>(from) + size > text_.size())
            return;
        silentAfterDelete_ = quirks_.reportsNothingAfterDelete || quirks_.repeatsOldTextAfterDelete;
        previousText_      = text_;
        previousCursor_    = cursor_;
        text_.erase(static_cast<size_t>(from), size);
        if (cursor_ > static_cast<size_t>(from))
            cursor_ = cursor_ >= static_cast<size_t>(from) + size ? cursor_ - size : static_cast<size_t>(from);
        anchor_ = cursor_;
        if (leaveIfEmptied())
            return;
        changed();
        redrawAfterEdit();
    }

    bool FakeApp::leaveIfEmptied() {
        if (!text_.empty() || quirks_.refocusWhenEmptiedMs <= 0)
            return false;
        leaveFocus(quirks_.refocusWhenEmptiedMs);
        return true;
    }

    void FakeApp::leaveFocus(int ms) {
        focused_ = false;
        state_->deactivate(true);
        later(ms, [this] {
            focused_ = true;
            state_->activate(Mode::Sen, true);
            changed();
        });
    }

    Surrounding FakeApp::surrounding() const {
        if (!field().surroundingText)
            return {};
        if (quirks_.reportsEmptyText)
            return Surrounding("", 0, 0);
        if (silentAfterDelete_)
            return Surrounding(encode(reportedText_), static_cast<unsigned int>(reportedCursor_), static_cast<unsigned int>(reportedAnchor_));
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

    void FakeApp::insert(const std::u32string& text, bool typed) {
        if (typed)
            silentAfterDelete_ = false;
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
        if (leaveIfEmptied())
            return;
        changed();
        redrawAfterEdit();
    }

    void FakeApp::changed() {
        shownCursor_ = cursor_;
        shownAnchor_ = anchor_;
        if (silentAfterDelete_ && !quirks_.repeatsOldTextAfterDelete)
            return;
        if (field_.surroundingText && quirks_.reports)
            later(quirks_.reportDelayMs, [this] {
                if (!silentAfterDelete_) {
                    reportedText_   = text_;
                    reportedCursor_ = cursor_;
                    reportedAnchor_ = anchor_;
                }
                inReport_ = true;
                state_->surroundingUpdated();
                inReport_ = false;
            });
    }

    void FakeApp::startTurn() {
        if (turnPending_)
            return;
        turnPending_ = true;
        later(0, [this] { endTurn(); });
    }

    void FakeApp::endTurn() {
        if (!turnPending_)
            return;
        turnPending_ = false;
        if (turnDeleteBytes_) {
            const unsigned int chars = charsForBytesBefore(*turnDeleteBytes_);
            eraseAround(-static_cast<int>(chars), chars);
        }
        if (turnCommit_ && focused_)
            insert(utf8::decode(*turnCommit_));
        turnDeleteBytes_.reset();
        turnCommit_.reset();
    }

    void FakeApp::later(int ms, std::function<void()> action) {
        timers_.push_back(loop_.add(monotonicUs() + static_cast<uint64_t>(ms) * 1000, [action = std::move(action)](Timer&) {
            action();
            return false;
        }));
    }

} // namespace ngosen::test
