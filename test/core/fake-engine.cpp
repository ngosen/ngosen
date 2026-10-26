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

} // namespace

namespace ngosen {

    // NGOSEN_TEST_LOG=1 shows the typing logic's log on stderr.
    bool logEnabled(LogLevel /*level*/) {
        static const bool enabled = std::getenv("NGOSEN_TEST_LOG") != nullptr;
        return enabled || capturing;
    }

    void writeLog(LogLevel /*level*/, const char* file, int line, const std::string& message) {
        if (capturing)
            test::capturedLog().push_back(message);
        else
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
