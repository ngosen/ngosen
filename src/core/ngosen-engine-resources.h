/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "emoji-entry.h"
#include "ngosen-options.h"
#include "ngosen-recorder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ngosen {

    struct KeymapEntry {
        std::string key;
        std::string value;
    };

    // What every input field's typing logic shares through the engine: settings, the tables the
    // Vietnamese engine reads, and the emoji list.
    class EngineResources {
      public:
        virtual ~EngineResources() = default;

        virtual const Options& options() const = 0;
        // Empty unless the custom keymap is turned on.
        virtual std::vector<KeymapEntry> customKeymap() const = 0;
        // Handles passed to the Vietnamese engine.
        virtual uintptr_t               dictionary() const = 0;
        virtual uintptr_t               macroTable() const = 0;

        virtual std::vector<EmojiEntry> emojiHistory()                         = 0;
        virtual std::vector<EmojiEntry> searchEmoji(const std::string& prefix) = 0;
        virtual void                    recordEmoji(const EmojiEntry& entry)   = 0;

        // Shared by every field, so a saved log follows the user across windows.
        virtual Recorder& recorder() = 0;
    };

} // namespace ngosen
