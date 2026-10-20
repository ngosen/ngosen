/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/dbus/objectvtable.h>

#include <string>

namespace fcitx {

    class NgoSenEngine;

    // For a terminal command, when the tray menu is missing or frozen.
    class NgoSenDBusObject : public dbus::ObjectVTable<NgoSenDBusObject> {
      public:
        static constexpr const char* Path      = "/ngosen";
        static constexpr const char* Interface = "io.github.ngosen.NgoSen1";

        explicit NgoSenDBusObject(NgoSenEngine* engine) : engine_(engine) {}

        std::string saveTypingLog();

      private:
        NgoSenEngine* engine_;
        FCITX_OBJECT_VTABLE_METHOD(saveTypingLog, "SaveTypingLog", "", "s");
    };

} // namespace fcitx
