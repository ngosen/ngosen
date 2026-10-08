/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-engine-resources.h"
#include "ngosen-key.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ngosen::test {

    // Settings as a fresh install has them, with the system dictionary and an empty macro table.
    class FakeResources final : public EngineResources {
      public:
        FakeResources();
        ~FakeResources() override;

        const Options& options() const override {
            return options_;
        }
        std::vector<KeymapEntry> customKeymap() const override {
            return {};
        }
        uintptr_t dictionary() const override {
            return dictionary_;
        }
        uintptr_t macroTable() const override {
            return macroTable_;
        }
        std::vector<EmojiEntry> emojiHistory() override {
            return {};
        }
        std::vector<EmojiEntry> searchEmoji(const std::string& /*prefix*/) override {
            return {};
        }
        void recordEmoji(const EmojiEntry& /*entry*/) override {}

      private:
        Options   options_;
        uintptr_t dictionary_ = 0;
        uintptr_t macroTable_ = 0;
    };

    class FakeKey final : public KeyPress {
      public:
        explicit FakeKey(uint32_t sym, bool release = false, uint32_t states = 0) : sym_(sym), states_(states), release_(release) {}

        uint32_t sym() const override {
            return sym_;
        }
        uint32_t states() const override {
            return states_;
        }
        bool isRelease() const override {
            return release_;
        }
        bool     isModifier() const override;
        bool     isBareShift() const override;
        uint32_t code() const override {
            return 0;
        }
        uint32_t time() const override {
            return 0;
        }
        bool hasModifier() const override {
            return states_ != 0;
        }
        bool        isCursorMove() const override;
        std::string name() const override {
            return std::to_string(appSym_);
        }
        void replaceSym(uint32_t sym) override {
            appSym_ = sym;
        }
        void accept() override {
            accepted_ = true;
        }

        bool accepted() const {
            return accepted_;
        }

      private:
        uint32_t sym_;
        uint32_t states_;
        bool     release_;
        uint32_t appSym_   = sym_;
        bool     accepted_ = false;
    };

} // namespace ngosen::test
