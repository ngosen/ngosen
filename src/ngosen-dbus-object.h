/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/dbus/objectvtable.h>

#include <cstdint>
#include <string>
#include <tuple>

namespace fcitx {

    class NgoSenEngine;

    // For a terminal command, when the tray menu is missing or frozen, and for the settings window.
    class NgoSenDBusObject : public dbus::ObjectVTable<NgoSenDBusObject> {
      public:
        static constexpr const char* Path      = "/ngosen";
        static constexpr const char* Interface = "io.github.ngosen.NgoSen1";

        explicit NgoSenDBusObject(NgoSenEngine* engine) : engine_(engine) {}

        std::string saveTypingLog();
        bool        gnomePortalAnswered();
        // For the settings window, which converts with the values it shows before they are saved.
        std::string convertText(const std::string& text, const std::string& from, const std::string& to, int32_t letterCase, bool stripDiacritics);
        // For a desktop shortcut; returns whether Vietnamese is on.
        bool toggleVietnamese();
        // For the Word guessing page. Results come as values, so the window tells them from a D-Bus error.
        std::tuple<std::string, std::string> diacriticsStatus();
        std::tuple<std::string, std::string> saveLearned();
        std::tuple<bool, std::string>        forgetLearned();

      private:
        NgoSenEngine* engine_;
        FCITX_OBJECT_VTABLE_METHOD(saveTypingLog, "SaveTypingLog", "", "s");
        FCITX_OBJECT_VTABLE_METHOD(gnomePortalAnswered, "GnomePortalAnswered", "", "b");
        FCITX_OBJECT_VTABLE_METHOD(convertText, "ConvertText", "sssib", "s");
        FCITX_OBJECT_VTABLE_METHOD(toggleVietnamese, "ToggleVietnamese", "", "b");
        FCITX_OBJECT_VTABLE_METHOD(diacriticsStatus, "DiacriticsStatus", "", "ss");
        FCITX_OBJECT_VTABLE_METHOD(saveLearned, "SaveLearned", "", "ss");
        FCITX_OBJECT_VTABLE_METHOD(forgetLearned, "ForgetLearned", "", "bs");
    };

} // namespace fcitx
