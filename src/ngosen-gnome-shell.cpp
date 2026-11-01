/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-gnome-shell.h"

#include "ngosen-log.h"

#include <regex>

#ifdef NGOSEN_HAS_DBUS
// Older fcitx5 does not install variant.h; distributions that old ship a GNOME without the bug.
#if __has_include(<fcitx-utils/dbus/variant.h>)
#define NGOSEN_READS_GNOME_SHELL
#include <fcitx-utils/dbus/matchrule.h>
#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/dbus/variant.h>

#include <vector>
#endif
#endif

namespace ngosen {

    bool gnomeShellDropsForwardedKeys(const std::string& version) {
        static const std::regex dropping("50\\.[0-3]");
        return std::regex_match(version, dropping);
    }

    const std::string& relayedAppName(const std::string& program, const std::string& frontend, const std::string* shellFocusedApp) {
        if (shellFocusedApp != nullptr && !shellFocusedApp->empty() && frontend == "ibus" && program == "gnome-shell")
            return *shellFocusedApp;
        return program;
    }

#ifdef NGOSEN_READS_GNOME_SHELL
    namespace {
        constexpr const char* ShellService     = "org.gnome.Shell";
        constexpr const char* ShellPath        = "/org/gnome/Shell";
        constexpr const char* ExtensionUuid    = "forward-keys@ngosen.github.io";
        constexpr double      ExtensionActive  = 1; // ExtensionState.ACTIVE
        constexpr uint64_t    ReplyTimeoutUs   = 2000000;
        constexpr const char* AppNamePath      = "/io/github/ngosen/AppName";
        constexpr const char* AppNameInterface = "io.github.ngosen.AppName";
    }

    GnomeShellWatcher::GnomeShellWatcher(fcitx::dbus::Bus* bus, std::function<void(const std::string&)> onFocusedApp) :
        bus_(bus), watcher_(*bus), on_focused_app_(std::move(onFocusedApp)) {
        // GNOME reports an extension's state as a double, which fcitx5 does not read by default.
        fcitx::dbus::VariantTypeRegistry::defaultRegistry().registerType<double>();
        owner_entry_ = watcher_.watchService(ShellService, [this](const std::string&, const std::string&, const std::string& owner) {
            version_.clear();
            extension_active_ = false;
            update();
            setFocusedApp("");
            if (!owner.empty()) {
                refresh();
                askFocusedApp();
            }
        });
        state_signal_ =
            bus_->addMatch(fcitx::dbus::MatchRule(ShellService, ShellPath, "org.gnome.Shell.Extensions", "ExtensionStateChanged"), [this](fcitx::dbus::Message& message) {
                std::string uuid;
                message >> uuid;
                if (uuid == ExtensionUuid) {
                    refresh();
                    askFocusedApp();
                }
                return true;
            });
        app_signal_ = bus_->addMatch(fcitx::dbus::MatchRule(ShellService, AppNamePath, AppNameInterface, "FocusedApp"), [this](fcitx::dbus::Message& message) {
            std::string app;
            message >> app;
            setFocusedApp(app);
            return true;
        });
    }

    // The answer comes as a FocusedApp signal, which only this process receives.
    void GnomeShellWatcher::askFocusedApp() {
        auto sync = bus_->createMethodCall(ShellService, AppNamePath, AppNameInterface, "Sync");
        app_call_ = sync.callAsync(ReplyTimeoutUs, [](fcitx::dbus::Message&) { return true; });
    }

    void GnomeShellWatcher::setFocusedApp(const std::string& app) {
        if (app == focused_app_)
            return;
        focused_app_ = app;
        on_focused_app_(app);
    }

    void GnomeShellWatcher::refresh() {
        auto version = bus_->createMethodCall(ShellService, ShellPath, "org.freedesktop.DBus.Properties", "Get");
        version << std::string(ShellService) << std::string("ShellVersion");
        version_call_ = version.callAsync(ReplyTimeoutUs, [this](fcitx::dbus::Message& reply) {
            fcitx::dbus::Variant value;
            if (!reply.isError())
                reply >> value;
            version_ = value.signature() == "s" ? value.dataAs<std::string>() : std::string();
            update();
            return true;
        });

        auto info = bus_->createMethodCall(ShellService, ShellPath, "org.gnome.Shell.Extensions", "GetExtensionInfo");
        info << std::string(ExtensionUuid);
        extension_call_ = info.callAsync(ReplyTimeoutUs, [this](fcitx::dbus::Message& reply) {
            std::vector<fcitx::dbus::DictEntry<std::string, fcitx::dbus::Variant>> fields;
            if (!reply.isError())
                reply >> fields;
            extension_active_ = false;
            for (const auto& field : fields) {
                if (field.key() == "state" && field.value().signature() == "d")
                    extension_active_ = field.value().dataAs<double>() == ExtensionActive;
            }
            update();
            return true;
        });
    }

    void GnomeShellWatcher::update() {
        const bool drops = gnomeShellDropsForwardedKeys(version_) && !extension_active_;
        if (drops == drops_keys_)
            return;
        drops_keys_ = drops;
        NGOSEN_INFO("GNOME Shell " + version_ + (drops ? " loses forwarded keys; deleting through the text in apps it relays" : " delivers forwarded keys"));
    }
#elif defined(NGOSEN_HAS_DBUS)
    GnomeShellWatcher::GnomeShellWatcher(fcitx::dbus::Bus* bus, std::function<void(const std::string&)> /*onFocusedApp*/) : bus_(bus), watcher_(*bus) {}
#endif

} // namespace ngosen
