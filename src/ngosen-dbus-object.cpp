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
#include "ngosen-ui-text.h"

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

    bool NgoSenDBusObject::toggleVietnamese() {
        const auto on = engine_->toggleFromShortcut();
        if (!on) {
            // Follows Ngó Sen's language setting, as its menu and notifications do.
            const std::string message = ::ngosen::uiText(N_("Ngó Sen is not in the current input method group. Add it to this group in fcitx5 settings."));
            NGOSEN_WARN(message);
            throw dbus::MethodCallError("org.freedesktop.DBus.Error.Failed", message.c_str());
        }
        return *on;
    }

    std::tuple<std::string, std::string> NgoSenDBusObject::diacriticsStatus() {
        auto [table, problem] = engine_->diacriticsStatus();
        return {table, problem};
    }

    std::tuple<std::string, std::string> NgoSenDBusObject::saveLearned() {
        auto [result, error] = engine_->saveLearned();
        return {result, error};
    }

    std::tuple<bool, std::string> NgoSenDBusObject::forgetLearned() {
        auto [ok, error] = engine_->forgetLearned();
        return {ok, error};
    }

    std::string NgoSenDBusObject::chatShorthandList() {
        return engine_->chatShorthandList();
    }

    std::tuple<bool, std::string> NgoSenDBusObject::expandShorthandAgain(const std::string& key) {
        auto [ok, error] = engine_->expandShorthandAgain(key);
        return {ok, error};
    }

    std::string NgoSenDBusObject::convertText(const std::string& text, const std::string& from, const std::string& to, int32_t letterCase, bool stripDiacritics) {
        const ngosen::UniqueCPtr<char> converted(ConvertText(text.c_str(), from.c_str(), to.c_str(), letterCase, stripDiacritics));
        if (!converted)
            throw dbus::MethodCallError("org.freedesktop.DBus.Error.InvalidArgs", "Unknown charset or letter case");
        return converted.get();
    }

} // namespace fcitx
