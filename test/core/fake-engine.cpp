/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fake-engine.h"

#include "bamboo-core.h"
#include "ngosen-keysym.h"
#include "ngosen-log.h"

#include <cstdio>
#include <fcntl.h>
#include <cstdlib>

namespace {

    bool capturing = false;

    // NGOSEN_REPLACE_RECORD names a file that gets each replacement's log line, so moving code can be
    // checked to change none of them.
    FILE* recordFile() {
        static FILE* file = [] {
            const char* path = std::getenv("NGOSEN_REPLACE_RECORD");
            return path != nullptr ? std::fopen(path, "w") : nullptr;
        }();
        return file;
    }

    bool isReplaceLine(const std::string& message) {
        return message.rfind("[DEBUG] Replace: ", 0) == 0;
    }

    uintptr_t wordList(const char* path) {
        const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
        return fd == -1 ? uintptr_t{0} : NewWordList(static_cast<uintptr_t>(fd));
    }

} // namespace

namespace ngosen {

    // NGOSEN_TEST_LOG=1 shows the typing logic's log on stderr.
    bool logEnabled(LogLevel /*level*/) {
        static const bool enabled = std::getenv("NGOSEN_TEST_LOG") != nullptr;
        return enabled || capturing || recordFile() != nullptr;
    }

    void writeLog(LogLevel /*level*/, const char* file, int line, const std::string& message) {
        static const bool shown = std::getenv("NGOSEN_TEST_LOG") != nullptr;
        if (recordFile() != nullptr && isReplaceLine(message))
            std::fprintf(recordFile(), "%s\n", message.c_str() + 8);
        if (capturing)
            test::capturedLog().push_back(message);
        else if (shown)
            std::fprintf(stderr, "%s:%d %s\n", file, line, message.c_str());
    }

} // namespace ngosen

namespace ngosen::test {

    void captureLog(bool on) {
        capturing = on;
        capturedLog().clear();
    }

    std::vector<std::string>& capturedLog() {
        static std::vector<std::string> lines;
        return lines;
    }

    FakeResources::FakeResources() {
        static const bool initialized = (Init(), true);
        (void)initialized;

        char* empty[] = {nullptr};
        macroTable_   = NewMacroTable(empty);
        // The engine loads the system dictionary even with the custom dictionary off.
        const int fd = ::open(NGOSEN_TEST_DICTIONARY, O_RDONLY | O_CLOEXEC);
        if (fd != -1)
            dictionary_ = NewDictionary(static_cast<uintptr_t>(fd));
        // Read once: tests build many of these, and the list never changes.
        static const uintptr_t englishWords = wordList(NGOSEN_TEST_ENGLISH_WORDS);
        static const uintptr_t keepWords    = wordList(NGOSEN_TEST_KEEP_WORDS);
        englishWords_                       = englishWords;
        keepWords_                          = keepWords;
    }

    void FakeResources::setMacros(const std::vector<std::pair<std::string, std::string>>& macros) {
        std::vector<char*> pairs;
        for (const auto& [key, value] : macros) {
            pairs.push_back(const_cast<char*>(key.c_str()));   //NOLINT
            pairs.push_back(const_cast<char*>(value.c_str())); //NOLINT
        }
        pairs.push_back(nullptr);
        DeleteObject(macroTable_);
        macroTable_ = NewMacroTable(pairs.data());
    }

    FakeResources::~FakeResources() {
        DeleteObject(macroTable_);
        DeleteObject(dictionary_);
    }

    bool FakeKey::isModifier() const {
        return sym_ >= key::Shift_L && sym_ <= 0xffee;
    }

    bool FakeKey::isBareShift() const {
        return (sym_ == key::Shift_L || sym_ == key::Shift_R) && states_ == 0;
    }

    bool FakeKey::isCursorMove() const {
        return appSym_ >= 0xff50 && appSym_ <= 0xff57;
    }

} // namespace ngosen::test
