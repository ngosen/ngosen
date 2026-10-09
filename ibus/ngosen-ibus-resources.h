/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-engine-resources.h"
#include "ngosen-ibus-config.h"

#include <ctime>

namespace ngosen {

    // Settings and tables every field shares. No macros, custom keymap or emoji yet.
    class IBusResources final : public EngineResources {
      public:
        IBusResources();
        ~IBusResources() override;
        IBusResources(const IBusResources&)            = delete;
        IBusResources& operator=(const IBusResources&) = delete;

        // Rereads the settings file when it changed; true when it did.
        bool reload();
        Mode mode() const {
            return settings_.mode;
        }

        const Options& options() const override {
            return settings_.options;
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
        IBusSettings settings_;
        timespec     loadedMtime_{-1, 0};
        uintptr_t    dictionary_ = 0;
        uintptr_t    macroTable_ = 0;
    };

} // namespace ngosen
