/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fake-host.h"

#include "ngosen-clock.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace ngosen::test {

    struct FakeLoop::Entry final : Timer {
        FakeLoop*                   loop;
        uint64_t                    deadlineUs;
        bool                        armed = true;
        std::function<bool(Timer&)> onTime;

        Entry(FakeLoop* l, uint64_t d, std::function<bool(Timer&)> f) : loop(l), deadlineUs(d), onTime(std::move(f)) {}
        ~Entry() override {
            loop->remove(this);
        }
        void rearm(uint64_t newDeadlineUs) override {
            deadlineUs = newDeadlineUs;
            armed      = true;
        }
    };

    std::unique_ptr<Timer> FakeLoop::add(uint64_t deadlineUs, std::function<bool(Timer&)> onTime) {
        auto entry = std::make_unique<Entry>(this, deadlineUs, std::move(onTime));
        entries_.push_back(entry.get());
        return entry;
    }

    void FakeLoop::remove(Entry* entry) {
        entries_.erase(std::remove(entries_.begin(), entries_.end(), entry), entries_.end());
    }

    void FakeLoop::pump(int ms) {
        for (int i = 0; i < ms; ++i) {
            // A callback may add or destroy timers, so pick one due timer at a time.
            for (bool fired = true; fired;) {
                fired = false;
                for (Entry* entry : entries_) {
                    if (entry->armed && entry->deadlineUs <= monotonicUs()) {
                        entry->armed = false;
                        entry->onTime(*entry);
                        fired = true;
                        break;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void FakeHost::showCandidates(const std::vector<std::string>& labels, int pageSize, std::function<void(size_t)> onPick) {
        labels_     = labels;
        onPick_     = std::move(onPick);
        candidates_ = CandidatePage{static_cast<int>(labels.size()), 0, pageSize, labels.empty() ? -1 : 0, static_cast<int>(labels.size()) > pageSize, false};
    }

    void FakeHost::highlightCandidate(int index) {
        if (candidates_)
            candidates_->cursor = index;
    }

    void FakeHost::pickCandidate(int index) {
        if (onPick_ && index >= 0 && static_cast<size_t>(index) < labels_.size())
            onPick_(static_cast<size_t>(index));
    }

    std::string FakeHost::keyText(uint32_t sym) const {
        // Printable ASCII and Latin-1 keysyms equal their code points; the tests type nothing else.
        if (sym < 0x20 || sym > 0xff || (sym >= 0x7f && sym < 0xa0))
            return {};
        if (sym < 0x80)
            return std::string(1, static_cast<char>(sym));
        return {static_cast<char>(0xc0 | (sym >> 6)), static_cast<char>(0x80 | (sym & 0x3f))};
    }

} // namespace ngosen::test
