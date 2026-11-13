/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "emoji-entry.h"
#include "ngosen-options.h"
#include "ngosen-pty.h"
#include "ngosen-recorder.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
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
        virtual uintptr_t dictionary() const = 0;
        virtual uintptr_t macroTable() const = 0;
        // English words a Backspace leaves as typed; 0 for none.
        virtual uintptr_t englishWords() const {
            return 0;
        }
        // Words a breaking key must not cut back to Vietnamese, like `axit`; 0 turns that off.
        virtual uintptr_t keepWords() const {
            return 0;
        }

        virtual std::vector<EmojiEntry> emojiHistory()                         = 0;
        virtual std::vector<EmojiEntry> searchEmoji(const std::string& prefix) = 0;
        virtual void                    recordEmoji(const EmojiEntry& entry)   = 0;

        // Shared by every field, so a saved log follows the user across windows.
        virtual Recorder& recorder() = 0;

        // Whether a terminal of the user hides input, as at a password prompt; tests play one without a pty.
        virtual bool anyTerminalHidesInput() const {
            return anyPtyHidesInput();
        }

        // Apps whose fields mishandled a deletion through their surrounding text. Kept for this session
        // only, and shared by every field, as an app opens a new one for each window.
        bool surroundingUntrusted(const std::string& frontend, const std::string& program) const {
            const auto app = apps_.find({frontend, program});
            return app != apps_.end() && app->second.surroundingUntrusted;
        }
        void markSurroundingUntrusted(const std::string& frontend, const std::string& program) {
            apps_[{frontend, program}].surroundingUntrusted = true;
        }

      private:
        // What an app showed in this session. The same app can delete differently over IBus and over
        // Wayland, so the frontend is part of the key.
        struct AppHistory {
            bool surroundingUntrusted = false;
        };
        std::map<std::pair<std::string, std::string>, AppHistory> apps_;
    };

} // namespace ngosen
