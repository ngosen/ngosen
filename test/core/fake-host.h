/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ngosen::test {

    // Timers of every FakeHost, fired by pump() on the real monotonic clock.
    class FakeLoop {
      public:
        struct Entry;

        std::unique_ptr<Timer> add(uint64_t deadlineUs, std::function<bool(Timer&)> onTime);
        // Fires due timers for about ms milliseconds, sleeping 1 ms between rounds.
        void pump(int ms);
        void remove(Entry* entry);

      private:
        std::vector<Entry*> entries_;
    };

    struct ForwardedKey {
        EditKey key;
        bool    release;
    };

    // An app field that records what the typing logic sends and reports the surrounding text the
    // test sets.
    class FakeHost final : public Host {
      public:
        FakeHost(FakeLoop& loop, Field field) : loop_(loop), field_(std::move(field)) {}

        void commitText(const std::string& text) override {
            commits_.push_back(text);
        }
        void forwardKey(EditKey key, bool release) override {
            forwarded_.push_back({key, release});
        }
        void deleteSurrounding(int offset, unsigned int size) override {
            deletes_.emplace_back(offset, size);
        }
        bool pressSystemKeys(int count) override {
            systemKeys_.push_back(count);
            return systemKeysWork_;
        }
        bool canPressSystemKeys() const override {
            return systemKeysWork_;
        }

        Surrounding surrounding() const override {
            return surrounding_;
        }
        Field field() const override {
            return field_;
        }
        bool hasFocus() const override {
            return true;
        }

        void showPreedit(const std::string& text, bool /*underline*/) override {
            preedit_ = text;
        }
        void clearPreedit() override {
            preedit_.clear();
        }
        void resetPanel() override {
            preedit_.clear();
            candidates_.reset();
        }
        void refreshPreedit() override {}
        void refreshPanel() override {}

        void showCandidates(const std::vector<std::string>& labels, int pageSize, std::function<void(size_t)> onPick) override;
        void hideCandidates() override {
            candidates_.reset();
        }
        std::optional<CandidatePage> candidates() const override {
            return candidates_;
        }
        void        highlightCandidate(int index) override;
        void        nextCandidatePage() override {}
        void        prevCandidatePage() override {}
        void        pickCandidate(int index) override;
        void        setStatus(const std::string& /*text*/) override {}
        std::string translate(const char* text) const override {
            return text;
        }

        std::unique_ptr<Timer> startTimer(uint64_t deadlineUs, uint64_t /*accuracyUs*/, std::function<bool(Timer&)> onTime) override {
            return loop_.add(deadlineUs, std::move(onTime));
        }

        std::string keyText(uint32_t sym) const override;

        // Test side.
        void setSurrounding(const std::string& text, unsigned int cursor) {
            surrounding_ = Surrounding(text, cursor, cursor);
        }
        void setSystemKeysWork(bool works) {
            systemKeysWork_ = works;
        }
        const std::vector<std::string>& commits() const {
            return commits_;
        }
        const std::vector<ForwardedKey>& forwarded() const {
            return forwarded_;
        }
        const std::vector<std::pair<int, unsigned int>>& deletes() const {
            return deletes_;
        }
        const std::vector<int>& systemKeys() const {
            return systemKeys_;
        }
        const std::string& preedit() const {
            return preedit_;
        }

      private:
        FakeLoop&                                 loop_;
        Field                                     field_;
        Surrounding                               surrounding_;
        bool                                      systemKeysWork_ = false;
        std::vector<std::string>                  commits_;
        std::vector<ForwardedKey>                 forwarded_;
        std::vector<std::pair<int, unsigned int>> deletes_;
        std::vector<int>                          systemKeys_;
        std::string                               preedit_;
        std::optional<CandidatePage>              candidates_;
        std::vector<std::string>                  labels_;
        std::function<void(size_t)>               onPick_;
    };

} // namespace ngosen::test
