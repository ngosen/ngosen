/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-engine-resources.h"
#include "ngosen-key.h"
#include "ngosen-keysym.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace ngosen::test {

    // While on, every log line, whatever its level, is kept here instead of printed.
    void                      captureLog(bool on);
    std::vector<std::string>& capturedLog();

    // Settings as a fresh install has them, with the system dictionary and an empty macro table.
    class FakeResources final : public EngineResources {
      public:
        FakeResources();
        ~FakeResources() override;

        const Options& options() const override {
            return options_;
        }
        Options& mutableOptions() {
            return options_;
        }
        std::vector<KeymapEntry> customKeymap() const override {
            return customKeymap_;
        }
        void setCustomKeymap(std::vector<KeymapEntry> keymap) {
            customKeymap_ = std::move(keymap);
        }
        uintptr_t dictionary() const override {
            return dictionary_;
        }
        uintptr_t macroTable() const override {
            return macroTable_;
        }
        uintptr_t englishWords() const override {
            return englishWords_;
        }
        uintptr_t keepWords() const override {
            return keepWords_;
        }
        uintptr_t chatShorthand() const override {
            return chatShorthand_;
        }
        uintptr_t diacritics() const override {
            return options_.guessDiacritics ? diacritics_ : 0;
        }
        // Gives Đoán dấu a file to learn into, so a test can ask what it learned; call before building a
        // TypingState.
        void learnDiacriticsInto(const std::string& path);
        // Counts the chat abbreviations taken back in a file of its own; call before building a TypingState.
        void keepShorthandIn(const std::string& path);
        // Saves what Đoán dấu learned since the last call; false when it learned nothing.
        bool learnedDiacritics();
        // Replaces the macro table; call before building a TypingState.
        void setMacros(const std::vector<std::pair<std::string, std::string>>& macros);
        bool anyTerminalHidesInput() const override {
            return terminalHidesInput;
        }
        bool      terminalHidesInput = false;
        Recorder& recorder() override {
            return recorder_;
        }
        int webTerminalSaves = 0;

      protected:
        void webTerminalAppsChanged() override {
            ++webTerminalSaves;
        }

      private:
        Options                  options_;
        std::vector<KeymapEntry> customKeymap_;
        uintptr_t                dictionary_    = 0;
        uintptr_t                macroTable_    = 0;
        uintptr_t                englishWords_  = 0;
        uintptr_t                keepWords_     = 0;
        uintptr_t                chatShorthand_ = 0;
        uintptr_t                ownShorthand_  = 0;
        uintptr_t                diacritics_    = 0;
        Recorder                 recorder_;
    };

    class FakeKey final : public KeyPress {
      public:
        explicit FakeKey(uint32_t sym, bool release = false, uint32_t states = 0, uint32_t time = 0) : sym_(sym), states_(states), release_(release), time_(time) {}

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
            return time_ != 0 ? sym_ : 0;
        }
        uint32_t time() const override {
            return time_;
        }
        bool hasModifier() const override {
            return (states_ & ~modifier::Repeat) != 0;
        }
        bool isCursorMove() const override;
        // As fcitx::Key::toString names a letter or digit; it spells out punctuation, which tests do not reach.
        std::string name() const override {
            return std::isalnum(static_cast<int>(appSym_)) != 0 && appSym_ < 0x80 ? std::string(1, static_cast<char>(appSym_)) : std::to_string(appSym_);
        }
        void replaceSym(uint32_t sym) override {
            appSym_ = sym;
        }
        void accept() override {
            accepted_ = true;
        }

        bool accepted() const override {
            return accepted_;
        }
        // The key the app gets when the typing logic lets it through.
        uint32_t appSym() const {
            return appSym_;
        }

      private:
        uint32_t sym_;
        uint32_t states_;
        bool     release_;
        uint32_t time_;
        uint32_t appSym_   = sym_;
        bool     accepted_ = false;
    };

} // namespace ngosen::test
