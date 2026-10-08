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

namespace ngosen {

    // NGOSEN_TEST_LOG=1 shows the typing logic's log on stderr.
    bool logEnabled(LogLevel /*level*/) {
        static const bool enabled = std::getenv("NGOSEN_TEST_LOG") != nullptr;
        return enabled;
    }

    void writeLog(LogLevel /*level*/, const char* file, int line, const std::string& message) {
        std::fprintf(stderr, "%s:%d %s\n", file, line, message.c_str());
    }

} // namespace ngosen

namespace ngosen::test {

    FakeResources::FakeResources() {
        static const bool initialized = (Init(), true);
        (void)initialized;

        // Defaults of lotus-config.h.
        options_.inputMethod                      = "Telex";
        options_.outputCharset                    = "Unicode";
        options_.spellCheck                       = true;
        options_.modernStyle                      = true;
        options_.freeMarking                      = true;
        options_.w2u                              = 1;
        options_.timeFormat                       = "%H:%M";
        options_.dateFormat                       = "%d/%m/%Y";
        options_.autoNonVnRestore                 = true;
        options_.ddFreeStyle                      = true;
        options_.enableMacro                      = true;
        options_.capitalizeMacro                  = true;
        options_.messengerSelectOvertype          = true;
        options_.waitSurroundingEvent             = true;
        options_.waitSurroundingMinPerKeyMs       = 8;
        options_.waitSurroundingTimeoutMs         = 50;
        options_.waitSurroundingShortMs           = 40;
        options_.waitSurroundingSettleMs          = 40;
        options_.waitSurroundingSettleFirstWordMs = 60;
        options_.waitSurroundingProbeEvery        = 4;
        options_.surrDeleteSleepMs                = 4;
        options_.surrCommitSleepMs                = 3;

        char* empty[] = {nullptr};
        macroTable_   = NewMacroTable(empty);
        // The engine loads the system dictionary even with the custom dictionary off.
        const int fd = ::open(NGOSEN_TEST_DICTIONARY, O_RDONLY | O_CLOEXEC);
        if (fd != -1)
            dictionary_ = NewDictionary(static_cast<uintptr_t>(fd));
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
