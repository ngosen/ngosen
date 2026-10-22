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

    std::string typeInApp(FakeResources& resources, const Field& field, AppQuirks quirks, const std::vector<uint32_t>& keys, const std::vector<int>& gapsMs, int* slowestCommitMs,
                          Mode mode) {
        realMode = mode;
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
            } else if (const auto text = keyText(down.appSym()); !text.empty())
                later(quirks_.lagMs, [this, text] { insert(utf8::decode(text)); });
        }

        if (quirks_.holdsKeysMs <= 0) {
            FakeKey up(sym, true);
            state_->keyEvent(up);
            return;
        }
        heldKey_ = sym;
        later(quirks_.holdsKeysMs, [this, sym] {
            if (heldKey_ != sym)
                return;
            heldKey_ = 0;
            FakeKey up(sym, true);
            state_->keyEvent(up);
        });
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
        later(quirks_.lagMs, [this, text] { insert(utf8::decode(text)); });
    }

    void FakeApp::forwardKey(EditKey key, bool release, uint32_t time) {
        if (key != EditKey::BackSpace || release)
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
                FakeKey up(key::BackSpace, true, 0, time);
                // The press is lost on a key still held; its release lets the key go.
                if (heldKey_ == key::BackSpace) {
                    heldKey_ = 0;
                    state_->keyEvent(up);
                    continue;
                }
                FakeKey down(key::BackSpace, false, i == 0 ? 0 : modifier::Repeat, time);
                state_->keyEvent(down);
                if (!down.accepted() && quirks_.dropsKeysUnderPreedit) {
                    later(1, [this] {
                        if (preedit_.empty())
                            eraseBefore(1);
                    });
                } else if (!down.accepted())
                    eraseBefore(1);
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
        if (!quirks_.redrawsOnlyOnDeletion)
            redrawAfterEdit();
    }

    void FakeApp::redrawAfterEdit() {
        if (quirks_.redrawAfterEditMs <= 0)
            return;
        ++redraws_;
        later(quirks_.redrawAfterEditMs, [this, mode = realMode.load()] {
            state_->deactivate(true);
            state_->activate(mode, true);
            --redraws_;
            changed();
        });
    }

    void FakeApp::eraseBefore(size_t count) {
        count = count < cursor_ ? count : cursor_;
        if (count == 0)
            return;
        previousText_   = text_;
        previousCursor_ = cursor_;
        text_.erase(cursor_ - count, count);
        cursor_ -= count;
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
