/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-dbus-object.h"

#include "bamboo-core.h"
#include "ngosen-engine.h"
#include "ngosen-go-object.h"
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

    bool NgoSenDBusObject::gnomePortalAnswered() {
        return engine_->gnomePortalAnswered();
    }

    std::string NgoSenDBusObject::convertText(const std::string& text, const std::string& from, const std::string& to, int32_t letterCase, bool stripDiacritics) {
        const ngosen::UniqueCPtr<char> converted(ConvertText(text.c_str(), from.c_str(), to.c_str(), letterCase, stripDiacritics));
        if (!converted)
            throw dbus::MethodCallError("org.freedesktop.DBus.Error.InvalidArgs", "Unknown charset or letter case");
        return converted.get();
    }

} // namespace fcitx
