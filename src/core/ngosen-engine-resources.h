/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-options.h"
#include "ngosen-pty.h"
#include "ngosen-recorder.h"
#include "ngosen-strings.h"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
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
        // Chat abbreviations expanded for everyone; 0 for none.
        virtual uintptr_t chatShorthand() const {
            return 0;
        }
        // Đoán dấu's table and personal dictionary, shared by every field; 0 while it is off.
        virtual uintptr_t diacritics() const {
            return 0;
        }

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
        // Apps that sent a copy that fits, so a field they open again is worth the longer copy wait.
        bool copyFitted(const std::string& frontend, const std::string& program) const {
            const auto app = apps_.find({frontend, program});
            return app != apps_.end() && app->second.copyFitted;
        }
        void noteCopyFitted(const std::string& frontend, const std::string& program) {
            apps_[{frontend, program}].copyFitted = true;
        }
        // Apps whose field kept our commits but never a letter typed into it, as xterm.js does.
        bool keepsOnlyCommits(const std::string& frontend, const std::string& program) const {
            const auto app = apps_.find({frontend, program});
            return app != apps_.end() && app->second.keepsOnlyCommits;
        }
        void noteKeepsOnlyCommits(const std::string& frontend, const std::string& program) {
            apps_[{frontend, program}].keepsOnlyCommits = true;
        }
        // Apps not named in the quirks that turned out to hold an xterm.js terminal. Kept across sessions,
        // so their first words come out right after a restart too.
        bool showedWebTerminal(const std::string& program) const {
            return webTerminalApps_.count(program) > 0;
        }
        void noteWebTerminal(const std::string& program) {
            if (!program.empty() && webTerminalApps_.insert(program).second)
                webTerminalAppsChanged();
        }
        // GNOME Shell's "window:N" names are never saved, so they stay until the session ends.
        void loadWebTerminalApps(const std::unordered_set<std::string>& saved) {
            for (auto it = webTerminalApps_.begin(); it != webTerminalApps_.end();)
                it = isStartsWith(*it, "window:") ? std::next(it) : webTerminalApps_.erase(it);
            webTerminalApps_.insert(saved.begin(), saved.end());
        }

      protected:
        virtual void                    webTerminalAppsChanged() {}
        std::unordered_set<std::string> webTerminalApps_;

      private:
        // What an app showed in this session. The same app can delete differently over IBus and over
        // Wayland, so the frontend is part of the key.
        struct AppHistory {
            bool surroundingUntrusted = false;
            bool copyFitted           = false;
            bool keepsOnlyCommits     = false;
        };
        std::map<std::pair<std::string, std::string>, AppHistory> apps_;
    };

} // namespace ngosen
