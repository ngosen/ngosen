/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-gnome-shell.h"

#include "ngosen-log.h"

#include <cstdio>
#include <regex>
#include <tuple>

#ifdef NGOSEN_HAS_DBUS
// Older fcitx5 does not install variant.h; distributions that old ship a GNOME without the bug.
#if __has_include(<fcitx-utils/dbus/variant.h>)
#define NGOSEN_READS_GNOME_SHELL
#include <fcitx-utils/dbus/matchrule.h>
#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/dbus/variant.h>

#include <algorithm>
#include <vector>
#ifdef NGOSEN_FCITX_SDBUS
#include <dlfcn.h>
#endif
#endif
#endif

namespace ngosen {

    bool gnomeShellDropsForwardedKeys(const std::string& version) {
        static const std::regex dropping("50\\.[0-3]");
        return std::regex_match(version, dropping);
    }

    bool fcitxNamesGnomeApps(const std::string& version) {
        int major = 0, minor = 0, patch = 0;
        if (std::sscanf(version.c_str(), "%d.%d.%d", &major, &minor, &patch) < 2)
            return false;
        return std::make_tuple(major, minor, patch) >= std::make_tuple(5, 1, 22);
    }

    const std::string& relayedAppName(const std::string& program, const std::string& frontend, const std::string* shellFocusedApp) {
        if (shellFocusedApp != nullptr && !shellFocusedApp->empty() && frontend == "ibus" && (program == "gnome-shell" || program.empty()))
            return *shellFocusedApp;
        return program;
    }

#ifdef NGOSEN_READS_GNOME_SHELL
    namespace {
        constexpr const char* ShellService        = "org.gnome.Shell";
        constexpr const char* ShellPath           = "/org/gnome/Shell";
        constexpr const char* ExtensionUuid       = "forward-keys@ngosen.github.io";
        constexpr double      ExtensionActive     = 1; // ExtensionState.ACTIVE
        constexpr uint64_t    ReplyTimeoutUs      = 2000000;
        constexpr const char* AppNamePath         = "/io/github/ngosen/AppName";
        constexpr const char* AppNameInterface    = "io.github.ngosen.AppName";
        constexpr const char* PortalService       = "org.freedesktop.impl.portal.desktop.gnome";
        constexpr const char* IntrospectInterface = "org.gnome.Shell.Introspect";
        constexpr const char* PropertiesInterface = "org.freedesktop.DBus.Properties";

        // fcitx5 before 5.1.22 has no Message::serial(); on sd-bus its native handle tells it.
        uint64_t messageSerial(const fcitx::dbus::Message& message, bool replySerial) {
#ifdef NGOSEN_FCITX_SDBUS
            using CookieGetter             = int (*)(void*, uint64_t*);
            static const auto  cookie      = reinterpret_cast<CookieGetter>(dlsym(RTLD_DEFAULT, "sd_bus_message_get_cookie"));
            static const auto  replyCookie = reinterpret_cast<CookieGetter>(dlsym(RTLD_DEFAULT, "sd_bus_message_get_reply_cookie"));
            const CookieGetter get         = replySerial ? replyCookie : cookie;
            uint64_t           serial      = 0;
            void*              handle      = message.nativeHandle();
            if (get != nullptr && handle != nullptr && get(handle, &serial) >= 0)
                return serial;
#else
            (void)message;
            (void)replySerial;
#endif
            return 0;
        }

        std::vector<IntrospectEntry> readIntrospectReply(fcitx::dbus::Message& message) {
            std::vector<fcitx::dbus::DictEntry<std::string, std::vector<fcitx::dbus::DictEntry<std::string, fcitx::dbus::Variant>>>> apps;
            message >> apps;
            std::vector<IntrospectEntry> entries;
            if (!message)
                return entries;
            for (const auto& app : apps) {
                IntrospectEntry entry{app.key(), {}};
                for (const auto& property : app.value()) {
                    if (property.key() == "active-on-seats" && property.value().signature() == "as")
                        entry.activeOnSeats = property.value().dataAs<std::vector<std::string>>();
                }
                entries.push_back(std::move(entry));
            }
            return entries;
        }
    } // namespace

    GnomeShellWatcher::GnomeShellWatcher(fcitx::dbus::Bus* bus, bool watchPortal, std::function<void(const std::string&)> onFocusedApp) :
        bus_(bus), watcher_(*bus), on_focused_app_(std::move(onFocusedApp)), watch_portal_(watchPortal) {
        // GNOME reports an extension's state as a double, which fcitx5 does not read by default.
        fcitx::dbus::VariantTypeRegistry::defaultRegistry().registerType<double>();
        fcitx::dbus::VariantTypeRegistry::defaultRegistry().registerType<std::vector<std::string>>();
        owner_entry_ = watcher_.watchService(ShellService, [this](const std::string&, const std::string&, const std::string& owner) {
            version_.clear();
            extension_active_ = false;
            update();
            names_.setExtensionRunning(false);
            names_.setOverviewActive(false);
            setOwners(owner, portal_owner_);
            if (!owner.empty()) {
                refresh();
                askFocusedApp();
                askOverview();
            }
        });
        if (watch_portal_)
            portal_entry_ = watcher_.watchService(PortalService, [this](const std::string&, const std::string&, const std::string& owner) { setOwners(shell_owner_, owner); });
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
            names_.setExtensionApp(app);
            publish();
            return true;
        });
        overview_signal_ =
            bus_->addMatch(fcitx::dbus::MatchRule(ShellService, ShellPath, PropertiesInterface, "PropertiesChanged", {ShellService}), [this](fcitx::dbus::Message& message) {
                std::string                                                            interface;
                std::vector<fcitx::dbus::DictEntry<std::string, fcitx::dbus::Variant>> changed;
                std::vector<std::string>                                               invalidated;
                message >> interface >> changed >> invalidated;
                for (const auto& property : changed) {
                    if (property.key() == "OverviewActive" && property.value().signature() == "b") {
                        names_.setOverviewActive(property.value().dataAs<bool>());
                        publish();
                    }
                }
                if (std::find(invalidated.begin(), invalidated.end(), "OverviewActive") != invalidated.end())
                    askOverview();
                return true;
            });
    }

    // The answer comes as a FocusedApp signal, which only this process receives.
    void GnomeShellWatcher::askFocusedApp() {
        auto sync = bus_->createMethodCall(ShellService, AppNamePath, AppNameInterface, "Sync");
        app_call_ = sync.callAsync(ReplyTimeoutUs, [](fcitx::dbus::Message&) { return true; });
    }

    void GnomeShellWatcher::askOverview() {
        auto get = bus_->createMethodCall(ShellService, ShellPath, PropertiesInterface, "Get");
        get << std::string(ShellService) << std::string("OverviewActive");
        overview_call_ = get.callAsync(ReplyTimeoutUs, [this](fcitx::dbus::Message& reply) {
            fcitx::dbus::Variant value;
            if (!reply.isError())
                reply >> value;
            if (value.signature() == "b") {
                names_.setOverviewActive(value.dataAs<bool>());
                publish();
            }
            return true;
        });
    }

    void GnomeShellWatcher::publish() {
        if (names_.name() == published_)
            return;
        published_ = names_.name();
        NGOSEN_INFO("GNOME focused app: " + (published_.empty() ? std::string("unknown") : published_) + ", from " + names_.source());
        on_focused_app_(published_);
    }

    void GnomeShellWatcher::setOwners(const std::string& shell, const std::string& portal) {
        // A restarted GNOME Shell or portal tells the app only at the next app change.
        if (shell != shell_owner_ || portal != portal_owner_)
            names_.dropPortalApp();
        shell_owner_  = shell;
        portal_owner_ = portal;
        matcher_.setOwners(shell, portal);
        startMonitor();
        publish();
    }

    // The portal asks GNOME Shell for the running apps whenever the focused one changes. A monitor
    // connection may not send anything, so this one only listens.
    void GnomeShellWatcher::startMonitor() {
        monitor_call_.reset();
        monitor_filter_.reset();
        monitor_bus_.reset();
        if (!watch_portal_ || shell_owner_.empty() || portal_owner_.empty())
            return;
        try {
            monitor_bus_ = std::make_unique<fcitx::dbus::Bus>(bus_->address());
        } catch (const std::exception& e) {
            NGOSEN_WARN(std::string("Could not connect to the session bus to watch GNOME's portal: ") + e.what());
            return;
        }
        monitor_bus_->attachEventLoop(bus_->eventLoop());
        monitor_filter_                       = monitor_bus_->addFilter([this](fcitx::dbus::Message& message) { return readMonitored(message); });
        const std::vector<std::string> rules  = {"type='method_call',sender='" + portal_owner_ + "',destination='" + shell_owner_ + "',interface='" + IntrospectInterface +
                                                     "',member='GetRunningApplications'",
                                                 "type='method_return',sender='" + shell_owner_ + "',destination='" + portal_owner_ + "'"};
        auto                           become = monitor_bus_->createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus.Monitoring", "BecomeMonitor");
        become << rules << static_cast<uint32_t>(0);
        monitor_call_ = become.callAsync(ReplyTimeoutUs, [](fcitx::dbus::Message& reply) {
            if (reply.isError())
                NGOSEN_WARN("Could not watch GNOME's portal for the focused app: " + reply.errorMessage());
            else
                NGOSEN_INFO("Watching GNOME's portal for the focused app");
            return true;
        });
    }

    bool GnomeShellWatcher::readMonitored(fcitx::dbus::Message& message) {
        // Calls stop here, or the bus library would answer them and the bus drop the monitor.
        if (message.type() == fcitx::dbus::MessageType::MethodCall) {
            if (message.member() == "GetRunningApplications")
                matcher_.call(message.sender(), message.destination(), messageSerial(message, false));
            return true;
        }
        if (message.type() != fcitx::dbus::MessageType::Reply || message.sender() != shell_owner_)
            return false;
        if (matcher_.answers(message.sender(), message.destination(), messageSerial(message, true), message.signature())) {
            names_.setPortalApps(runningApps(readIntrospectReply(message)));
            publish();
        }
        return true;
    }

    void GnomeShellWatcher::refresh() {
        auto version = bus_->createMethodCall(ShellService, ShellPath, PropertiesInterface, "Get");
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
            names_.setExtensionRunning(extension_active_);
            publish();
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
    GnomeShellWatcher::GnomeShellWatcher(fcitx::dbus::Bus* bus, bool /*watchPortal*/, std::function<void(const std::string&)> /*onFocusedApp*/) : bus_(bus), watcher_(*bus) {}
#endif

} // namespace ngosen
