/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

namespace ngosen {

    // The modifier whose tap keeps the word being typed as it is.
    enum class LockWordKey {
        None,
        Shift,
        Ctrl,
        Alt,
    };

    // The settings the typing logic reads, as plain values. The framework copies them from its own
    // configuration each time that changes. The initial values are the defaults every frontend starts from.
    struct Options {
        std::string inputMethod                    = "Telex";
        std::string outputCharset                  = "Unicode";
        bool        spellCheck                     = true;
        bool        modernStyle                    = true;
        bool        freeMarking                    = true;
        int         w2u                            = 1;
        int         bracketTransform               = 0;
        std::string timeFormat                     = "%H:%M";
        std::string dateFormat                     = "%d/%m/%Y";
        bool        autoNonVnRestore               = true;
        bool        ddFreeStyle                    = true;
        bool        backspaceRemovesTone           = false;
        bool        enableMacro                    = true;
        bool        enableMacroInOffMode           = false;
        bool        capitalizeMacro                = true;
        LockWordKey lockWordKey                    = LockWordKey::Ctrl;
        bool        autoCapitalizeAfterPunctuation = false;
        bool        capitalizeStartOfText          = false;
        bool        doubleSpaceToPeriod            = false;
        bool        doubleHyphenToEmDash           = false;
        bool        retypeAfterSwitch              = true;
        bool        retoneSelection                = true;

        // How old text is replaced in the app.
        bool messengerSelectOvertype          = true;
        bool waitSurroundingEvent             = true;
        int  waitSurroundingMinPerKeyMs       = 8;
        int  waitSurroundingTimeoutMs         = 50;
        int  waitSurroundingShortMs           = 40;
        int  waitSurroundingSettleMs          = 40;
        int  waitSurroundingSettleFirstWordMs = 60;
        int  waitSurroundingProbeEvery        = 4;
        int  surrDeleteSleepMs                = 4;
        int  surrCommitSleepMs                = 3;
    };

    inline const Options defaultOptions{};

} // namespace ngosen
