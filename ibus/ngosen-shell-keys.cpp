/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-shell-keys.h"

#include "ngosen-log.h"

#include <gio/gio.h>

namespace ngosen::shell {

    namespace {

        constexpr const char* Name      = "io.github.ngosen.Shell";
        constexpr const char* Path      = "/io/github/ngosen/Shell";
        constexpr const char* Interface = "io.github.ngosen.Shell";
        // The extension answers at once; a slow answer means GNOME Shell is stuck.
        constexpr int TimeoutMs = 500;

        // Takes ownership of args; returns the reply, or null after logging why.
        GVariant* call(const char* method, GVariant* args, const char* replyType) {
            GError*          error = nullptr;
            GDBusConnection* bus   = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
            GVariant*        reply = nullptr;
            if (bus != nullptr) {
                reply =
                    g_dbus_connection_call_sync(bus, Name, Path, Interface, method, args, G_VARIANT_TYPE(replyType), G_DBUS_CALL_FLAGS_NO_AUTO_START, TimeoutMs, nullptr, &error);
                g_object_unref(bus);
            } else if (args != nullptr) {
                g_variant_unref(g_variant_ref_sink(args));
            }
            if (reply == nullptr) {
                NGOSEN_DEBUG("GNOME Shell extension: " << method << " failed: " << (error != nullptr ? error->message : "no session bus"));
                g_clear_error(&error);
            }
            return reply;
        }

    } // namespace

    std::string focusedX11Class() {
        GVariant* reply = call("FocusedX11Class", nullptr, "(s)");
        if (reply == nullptr)
            return {};
        const gchar* wmClass = nullptr;
        g_variant_get(reply, "(&s)", &wmClass);
        std::string result = wmClass;
        g_variant_unref(reply);
        return result;
    }

    bool pressBackSpace(int count) {
        GVariant* reply = call("PressBackSpace", g_variant_new("(u)", static_cast<guint32>(count)), "(b)");
        if (reply == nullptr)
            return false;
        gboolean pressed = FALSE;
        g_variant_get(reply, "(b)", &pressed);
        g_variant_unref(reply);
        return pressed != FALSE;
    }

} // namespace ngosen::shell
