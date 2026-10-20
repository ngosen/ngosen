/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-dbus-object.h"

#include "ngosen-engine.h"
#include "ngosen-log.h"

#include <fcitx-utils/i18n.h>

namespace fcitx {

    std::string NgoSenDBusObject::saveTypingLog() {
        const auto path = engine_->writeTypingLog();
        if (path.empty()) {
            const std::string message = _("Could not save the typing log to ") + NgoSenEngine::typingLogDir().string();
            NGOSEN_WARN(message);
            throw dbus::MethodCallError("org.freedesktop.DBus.Error.Failed", message.c_str());
        }
        NGOSEN_INFO(_("Typing log saved: ") + path.string());
        return path.string();
    }

} // namespace fcitx
