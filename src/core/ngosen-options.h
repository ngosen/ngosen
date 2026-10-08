/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

namespace ngosen {

    // The modifier whose tap types the next word without macros.
    enum class MacroSkipKey {
        None,
        Shift,
        Ctrl,
        Alt,
    };

    // The settings the typing logic reads, as plain values. The framework copies them from its own
    // configuration each time that changes.
    struct Options {
        std::string  inputMethod;
        std::string  outputCharset;
        bool         spellCheck       = false;
        bool         modernStyle      = false;
        bool         freeMarking      = false;
        int          w2u              = 0;
        int          bracketTransform = 0;
        std::string  timeFormat;
        std::string  dateFormat;
        bool         autoNonVnRestore               = false;
        bool         ddFreeStyle                    = false;
        bool         enableMacro                    = false;
        bool         enableMacroInOffMode           = false;
        bool         capitalizeMacro                = false;
        MacroSkipKey macroSkipKey                   = MacroSkipKey::None;
        bool         autoCapitalizeAfterPunctuation = false;
        bool         doubleSpaceToPeriod            = false;
        bool         doubleHyphenToEmDash           = false;

        // How old text is replaced in the app.
        bool useSurroundingTextIfPossible     = false;
        bool messengerSelectOvertype          = false;
        bool waitSurroundingEvent             = false;
        int  waitSurroundingMinPerKeyMs       = 0;
        int  waitSurroundingTimeoutMs         = 0;
        int  waitSurroundingShortMs           = 0;
        int  waitSurroundingSettleMs          = 0;
        int  waitSurroundingSettleFirstWordMs = 0;
        int  waitSurroundingProbeEvery        = 0;
        int  surrDeleteSleepMs                = 0;
        int  surrCommitSleepMs                = 0;
    };

} // namespace ngosen
