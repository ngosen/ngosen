/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>

// The keys and modifiers the typing logic checks for, as X11 keysym numbers and modifier bits.
// fcitx5 and IBus both use these numbers, so the typing logic needs no framework header.
namespace ngosen::key {

    inline constexpr uint32_t space        = 0x0020;
    inline constexpr uint32_t exclam       = 0x0021;
    inline constexpr uint32_t asterisk     = 0x002a;
    inline constexpr uint32_t plus         = 0x002b;
    inline constexpr uint32_t minus        = 0x002d;
    inline constexpr uint32_t period       = 0x002e;
    inline constexpr uint32_t slash        = 0x002f;
    inline constexpr uint32_t Digit0       = 0x0030;
    inline constexpr uint32_t Digit1       = 0x0031;
    inline constexpr uint32_t Digit9       = 0x0039;
    inline constexpr uint32_t equal        = 0x003d;
    inline constexpr uint32_t question     = 0x003f;
    inline constexpr uint32_t A            = 0x0041;
    inline constexpr uint32_t a            = 0x0061;
    inline constexpr uint32_t z            = 0x007a;
    inline constexpr uint32_t ISO_Left_Tab = 0xfe20;
    inline constexpr uint32_t BackSpace    = 0xff08;
    inline constexpr uint32_t Tab          = 0xff09;
    inline constexpr uint32_t Return       = 0xff0d;
    inline constexpr uint32_t Escape       = 0xff1b;
    inline constexpr uint32_t Left         = 0xff51;
    inline constexpr uint32_t Up           = 0xff52;
    inline constexpr uint32_t Right        = 0xff53;
    inline constexpr uint32_t Down         = 0xff54;
    inline constexpr uint32_t Page_Up      = 0xff55;
    inline constexpr uint32_t Page_Down    = 0xff56;
    inline constexpr uint32_t KP_Space     = 0xff80;
    inline constexpr uint32_t KP_Tab       = 0xff89;
    inline constexpr uint32_t KP_Enter     = 0xff8d;
    inline constexpr uint32_t KP_Multiply  = 0xffaa;
    inline constexpr uint32_t KP_Add       = 0xffab;
    inline constexpr uint32_t KP_Subtract  = 0xffad;
    inline constexpr uint32_t KP_Decimal   = 0xffae;
    inline constexpr uint32_t KP_Divide    = 0xffaf;
    inline constexpr uint32_t KP_0         = 0xffb0;
    inline constexpr uint32_t KP_9         = 0xffb9;
    inline constexpr uint32_t KP_Equal     = 0xffbd;
    inline constexpr uint32_t Shift_L      = 0xffe1;
    inline constexpr uint32_t Shift_R      = 0xffe2;
    inline constexpr uint32_t Control_L    = 0xffe3;
    inline constexpr uint32_t Control_R    = 0xffe4;
    inline constexpr uint32_t Alt_L        = 0xffe9;
    inline constexpr uint32_t Alt_R        = 0xffea;
    inline constexpr uint32_t Delete       = 0xffff;

    // Some apps report BackSpace as the ASCII control code instead of the keysym.
    inline bool isBackspace(uint32_t sym) {
        return sym == BackSpace || sym == 8;
    }

} // namespace ngosen::key

namespace ngosen::modifier {

    inline constexpr uint32_t Ctrl = 1U << 2;

} // namespace ngosen::modifier
