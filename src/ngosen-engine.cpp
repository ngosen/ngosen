/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "ngosen-engine.h"
#include "fcitx-utils/keysym.h"
#include "ngosen-config.h"
#include "ngosen-state.h"
#include "ngosen-candidates.h"
#include "ngosen-monitor.h"
#include "ngosen-utils.h"
#include "ngosen-gnome-theme.h"
#include "ngosen-icon-resolver.h"
#include "ngosen-menu-icon.h"
#include "ngosen-plasma-theme.h"
#include "ngosen-fcitx-host.h"
#include "ngosen-migration.h"
#include "ngosen-ui-text.h"
#ifdef NGOSEN_HAS_DBUS
#include "ngosen-dbus-object.h"
#include "ngosen-gnome-shell.h"
#endif
#include <optional>
#include <utility>

#include <fcitx-config/iniparser.h>
#include <fcitx/globalconfig.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/menu.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/misc.h>
#include <fcitx-utils/fs.h>
#include <fcitx-utils/unixfd.h>
#ifdef NGOSEN_HAS_DBUS
#include <dbus_public.h>
#endif
#ifdef NGOSEN_HAS_NOTIFICATIONS
#include <notifications_public.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// Ngó Sen's own strings follow its language setting, not the language fcitx5 runs in.
#undef _
#define _(x) ::ngosen::uiText(x)

namespace fcitx {
    constexpr const char* CharsetActionId     = "ngosen-charset";
    constexpr const char* InputMethodActionId = "ngosen-input-method";
    const std::string     CustomKeymapFile    = "conf/ngosen-custom-keymap.conf";
    const std::string     MacroTableFile      = "conf/ngosen-macro-table.conf";
    const std::string     AppSwitchFile       = "conf/ngosen-app-switch.conf";

    int                   modeToInt(NgoSenMode mode) {
        switch (mode) {
            case NgoSenMode::Off: return 0;
            case NgoSenMode::Sen: return 2;
            case NgoSenMode::Preedit: return 5;
            case NgoSenMode::Emoji: return 6;
            default: return 0;
        }
    }

    NgoSenMode intToMode(int mode) {
        switch (mode) {
            case 0: return NgoSenMode::Off;
            case 1: // former Smooth
            case 2:
            case 3: // former Super Smooth
            case 4: // former Surrounding Text
            case 8: // former Minecraft
                return NgoSenMode::Sen;
            case 5: return NgoSenMode::Preedit;
            case 6: return NgoSenMode::Emoji;
            default: return NgoSenMode::Off;
        }
    }

    namespace {
        // deepin-editor stalls between edits, and fcitx5-qt applies a commit before the keys let
        // through ahead of it, so fast typing in Sen scrambles words there. Preedit sends no keys.
        bool typesInPreeditByDefault(const std::string& appName) {
            return stripDesktopSuffix(appName) == "deepin-editor";
        }

        bool otherModifierHeld(const Key& key) {
            return (key.states() & ~Key::keySymToStates(key.sym()) & KeyStates{KeyState::Ctrl_Alt_Shift, KeyState::Super}) != 0;
        }

        // Sen was called Uinput, and before that Smooth, Slow, Super Smooth and Minecraft; Surrounding Text
        // was merged into it. Their stored names would not parse, and fcitx would silently fall back to the
        // Preedit default.
        bool isFormerSenModeName(const std::string& name) {
            return name == "Uinput" || name == "Uinput (Smooth)" || name == "Uinput (Slow)" || name == "Uinput (Super Smooth)" || name == "Minecraft" || name == "Surrounding Text";
        }

        // Keeping several former names would show Sen several times in the mode menu.
        void migrateLegacyModeOrder(RawConfig& config) {
            const auto* order = config.valueByPath("ModeOrder");
            if (order == nullptr) {
                return;
            }
            std::vector<std::string> migrated;
            for (auto name : stringutils::split(*order, ",")) {
                if (name == "Uinput" || name == "Smooth" || name == "SuperSmooth" || name == "Minecraft" || name == "SurroundingText") {
                    name = "Sen";
                }
                if (std::find(migrated.begin(), migrated.end(), name) == migrated.end()) {
                    migrated.push_back(std::move(name));
                }
            }
            config.setValueByPath("ModeOrder", stringutils::join(migrated, ","));
        }

        void moveOption(RawConfig& config, const std::string& from, const std::string& to) {
            const auto* value = config.valueByPath(from);
            if (value != nullptr && config.valueByPath(to) == nullptr) {
                config.setValueByPath(to, *value);
            }
        }

        std::string userConfigDir() {
#if NGOSEN_USE_MODERN_FCITX_API
            return (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
            return StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif
        }

        void migrateLegacyMode(RawConfig& config) {
            if (const auto* mode = config.valueByPath("Mode"); mode != nullptr && isFormerSenModeName(*mode)) {
                config.setValueByPath("Mode", "Sen");
            }
            migrateLegacyModeOrder(config);
            moveOption(config, "ShowModeUinput", "ShowModeSen");
            moveOption(config, "ShortcutUinput", "ShortcutSen");
            // The key that skipped macros now keeps the whole word; the old default, Disabled, gives way to Ctrl.
            if (const auto* skip = config.valueByPath("MacroSkipTriggerModifier"); skip != nullptr && *skip != "Disabled")
                moveOption(config, "MacroSkipTriggerModifier", "LockWordModifier");
        }

        bool isSwitchKey(const GlobalConfig& config, const Key& key) {
            for (const auto* keys : {&config.triggerKeys(), &config.altTriggerKeys(), &config.activateKeys(), &config.deactivateKeys(), &config.enumerateForwardKeys(),
                                     &config.enumerateBackwardKeys(), &config.enumerateGroupForwardKeys(), &config.enumerateGroupBackwardKeys()}) {
                if (key.checkKeyList(*keys))
                    return true;
            }
            return false;
        }
    } // namespace

    // Returns the KeySym that triggers the "Type hotkey char" action in the mode
    // menu.  If the hotkey itself conflicts with a reserved menu key, falls back
    // to FcitxKey_f.
    static bool isAppModeMenuReservedKey(KeySym sym, const ngosenConfig& config) {
        if (sym == Key(*config.shortcutSen).sym() || sym == Key(*config.shortcutPreedit).sym() || sym == Key(*config.shortcutEmoji).sym() ||
            sym == Key(*config.shortcutOff).sym() || sym == Key(*config.shortcutDefault).sym()) {
            return true;
        }

        switch (sym) {
            case FcitxKey_Escape:
            case FcitxKey_Tab:
            case FcitxKey_ISO_Left_Tab:
            case FcitxKey_Return:
            case FcitxKey_space:
            case FcitxKey_Up:
            case FcitxKey_Down: return true;
            default: return false;
        }
    }

    static KeySym typeKeyForModeMenuHotkey(KeySym hotkeySym, const ngosenConfig& config) {
        return isAppModeMenuReservedKey(hotkeySym, config) ? FcitxKey_f : hotkeySym;
    }

    bool NgoSenEngine::isDarkMode() {
        // Each probe spawns subprocesses, and subModeIconImpl calls this on
        // every tray update while IconTheme is Auto.  Cache the result briefly
        // so the cost is paid at most once per few seconds.
        static int64_t lastCheckMs = 0;
        static bool    cachedValue = false;
        const int64_t  now         = now_ms();
        if (now - lastCheckMs < 5000) {
            return cachedValue;
        }
        lastCheckMs = now;
        cachedValue = false;

        // KDE Plasma: the tray sits on the panel, painted by the Plasma Style,
        // not by the application colour scheme.  The two differ in the
        // default Fedora/Kubuntu look (#374).
        if (isKdePlasmaSession(getEnv("XDG_CURRENT_DESKTOP"))) {
            if (const auto dark = isPlasmaPanelDark(plasmaThemeSearchPathsFromEnv())) {
                cachedValue = *dark;
                return cachedValue;
            }
        }

        // GNOME Shell: same mismatch, the top bar follows the shell stylesheet
        // (Ubuntu's Yaru bar is near-black under the light scheme).
        if (isGnomeShellSession(getEnv("XDG_CURRENT_DESKTOP"))) {
            if (const auto dark = isGnomePanelDark(gnomeShellThemeInfoFromEnv())) {
                cachedValue = *dark;
                return cachedValue;
            }
        }

        // Cinnamon: Linux Mint pairs a dark panel theme with light apps.
        if (isCinnamonSession(getEnv("XDG_CURRENT_DESKTOP"), getEnv("DESKTOP_SESSION"))) {
            if (const auto dark = isCinnamonPanelDark(cinnamonThemeInfoFromEnv())) {
                cachedValue = *dark;
                return cachedValue;
            }
        }

        // Elsewhere the app colour scheme says nothing about the panel, and
        // Xfce, MATE, LXQt, Budgie and tiling-WM bars mostly ship dark ones.
        cachedValue = true;
        return cachedValue;
    }

    static inline uintptr_t newMacroTable(const ngosenMacroTable& macroTable) {
        const auto&        macros = *macroTable.macros;
        std::vector<char*> charArray;
        charArray.reserve((macros.size() * 2) + 1);
        for (const auto& keymap : macros) {
            // External C API doesn't use const, but doesn't modify data
            charArray.push_back(const_cast<char*>(keymap.key->data()));   //NOLINT
            charArray.push_back(const_cast<char*>(keymap.value->data())); //NOLINT
        }
        charArray.push_back(nullptr);
        return NewMacroTable(charArray.data());
    }

    static inline std::vector<std::string> convertToStringList(char** list) {
        std::vector<std::string> result;
        if (list != nullptr) {
            for (size_t i = 0; list[i] != nullptr; ++i) { //NOLINT
                result.emplace_back(list[i]);             //NOLINT
                free(list[i]);                            //NOLINT
            }
            free(list); //NOLINT
        }
        return result;
    }

    uintptr_t NgoSenEngine::macroTable() const {
        if (config_.inputMethod.value().empty()) {
            return 0;
        }
        return macroTableObject_.handle();
    }

    TypingStateProperty::TypingStateProperty(std::unique_ptr<ngosen::TypingState> state) : state_(std::move(state)) {}

    TypingStateProperty::~TypingStateProperty() = default;

    namespace {
        // Without a name that tells windows apart, a rule is kept per context and never saved.
        // GNOME Shell relays every app under its own name until fcitx5 5.1.22.
        bool namesNoWindow(const std::string& program) {
            return program.empty() || program == "wayland" || program == "x11" || program == "gnome-shell";
        }

        std::string contextRuleName(InputContext* ic) {
            std::ostringstream oss;
            oss << "ctx_" << static_cast<const void*>(ic);
            return oss.str();
        }
    } // namespace

    ngosen::TypingState* NgoSenEngine::stateFor(InputContext* ic) {
        return &ic->propertyFor(&factory_)->state();
    }

    NgoSenEngine::NgoSenEngine(Instance* instance) :
        instance_(instance), factory_([this](InputContext& ic) {
            return new TypingStateProperty(std::make_unique<ngosen::TypingState>(this, std::make_unique<ngosen::FcitxHost>(&ic, instance_, shellDropsKeys(), &shellFocusedApp_)));
        }) { //NOLINT
        Init();
        {
            auto imNames = convertToStringList(GetInputMethodNames());
            imNames.push_back("Custom");
            imNames_ = std::move(imNames);
        }
        config_.inputMethod.annotation().setList(imNames_);
        cursorJumpWatcher_ = instance_->watchEvent(EventType::InputContextSurroundingTextUpdated, EventWatcherPhase::Default, [this](Event& event) {
            auto* ic = static_cast<InputContextEvent&>(event).inputContext();
            stateFor(ic)->surroundingUpdated();
        });
        commitWatcher_     = instance_->watchEvent(EventType::InputContextCommitString, EventWatcherPhase::Default, [this](Event& event) {
            auto& commit = static_cast<CommitStringEvent&>(event);
            stateFor(commit.inputContext())->noteCommit(commit.text());
        });
        // Text typed in another input method of the group, before fcitx5 switches to Ngó Sen; in
        // English Ngó Sen notes the keys itself.
        keyWhileOffWatcher_ = instance_->watchEvent(EventType::InputContextKeyEvent, EventWatcherPhase::PreInputMethod, [this](Event& event) {
            auto& keyEvent = static_cast<KeyEvent&>(event);
            auto* ic       = keyEvent.inputContext();
            // The switch key is usually Ctrl+Space; as a shortcut it would wipe the text it is about to offer.
            if (instance_->inputMethod(ic) == "ngosen" || isSwitchKey(instance_->globalConfig(), keyEvent.rawKey()))
                return;
            ngosen::FcitxKeyPress press(keyEvent);
            stateFor(ic)->noteKeyWhileOff(press);
        });
        leaveTextWatchers_.push_back(instance_->watchEvent(EventType::InputContextFocusOut, EventWatcherPhase::Default,
                                                           [this](Event& event) { stateFor(static_cast<InputContextEvent&>(event).inputContext())->forgetKeysWhileOff(); }));
        leaveTextWatchers_.push_back(instance_->watchEvent(EventType::InputContextReset, EventWatcherPhase::Default,
                                                           [this](Event& event) { stateFor(static_cast<InputContextEvent&>(event).inputContext())->fieldResetWhileOff(); }));
        // A ctx_ rule names a context address, which a later window may reuse.
        contextDestroyedWatcher_ = instance_->watchEvent(EventType::InputContextDestroyed, EventWatcherPhase::Default, [this](Event& event) {
            auto* ic = static_cast<InputContextEvent&>(event).inputContext();
            if (namesNoWindow(ic->program())) {
                const std::string appName = contextRuleName(ic);
                clearAppRule(appName);
                clearAppSwitch(appName);
            }
        });

        auto& uiManager = instance_->userInterfaceManager();

        // The menu offers the two methods most people switch between; the rest stay in the settings window.
        initChoiceMenu(
            inputMethodAction_, inputMethodSubAction_, inputMethodMenu_, InputMethodActionId, _("Input Method"), "input-keyboard", {"Telex", "VNI"},
            [this](InputContext* ic, const std::string& name) {
                if (config_.inputMethod.value() == name)
                    return;
                config_.inputMethod.setValue(name);
                syncOptions();
                saveConfig();
                refreshEngine();
                updateInputMethodAction(ic);
                if (ic)
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
            },
            uiManager);

        // Only Breeze has three of these icons; other themes get the copies Ngó Sen installs in hicolor.
        auto charsets = convertToStringList(GetCharsetNames());
        initChoiceMenu(
            charsetAction_, charsetSubAction_, charsetMenu_, CharsetActionId, _("Charset"), menuIconName("character-set", "ngosen-charset"), charsets,
            [this](InputContext* ic, const std::string& charset) {
                if (config_.outputCharset.value() == charset)
                    return;
                config_.outputCharset.setValue(charset);
                syncOptions();
                saveConfig();
                refreshEngine();
                updateCharsetAction(ic);
                if (ic)
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
            },
            uiManager);
        config_.outputCharset.annotation().setList(charsets);

        initToggleAction(spellCheckAction_, config_.spellCheck, "ngosen-spellcheck", "tools-check-spelling", _("Spell Check"), N_("Spell Check"), uiManager);
        initToggleAction(macroAction_, config_.enableMacro, "ngosen-macro", "document-edit", _("Macro"), N_("Macro"), uiManager);
        initToggleAction(capitalizeMacroAction_, config_.capitalizeMacro, "ngosen-capitalizemacro", menuIconName("format-text-uppercase", "ngosen-capitalize-macro"),
                         _("Capitalize Macro"), N_("Capitalize Macro"), uiManager);
        autoCapitalizeAction_ = std::make_unique<SimpleAction>();
        autoCapitalizeAction_->setShortText(_("Auto Capitalize"));
        autoCapitalizeAction_->setIcon(menuIconName("format-text-capitalize", "ngosen-auto-capitalize"));
        uiManager.registerAction("ngosen-autocapitalize", autoCapitalizeAction_.get());
        autoCapitalizeMenu_ = std::make_unique<Menu>();
        autoCapitalizeAction_->setMenu(autoCapitalizeMenu_.get());
        initToggleAction(capitalizeSentencesAction_, config_.autoCapitalizeAfterPunctuation, "ngosen-capitalize-sentences", "", _("After . ! ? and Enter"),
                         N_("After . ! ? and Enter"), uiManager);
        initToggleAction(capitalizeStartAction_, config_.capitalizeStartOfText, "ngosen-capitalize-start", "", _("Start of text or line"), N_("Start of text or line"), uiManager);
        autoCapitalizeMenu_->addAction(capitalizeSentencesAction_.get());
        autoCapitalizeMenu_->addAction(capitalizeStartAction_.get());
        initToggleAction(autoNonVnRestoreAction_, config_.autoNonVnRestore, "ngosen-autonvnrestore", "edit-undo", _("Auto Restore Invalid Words"), N_("Auto Non-VN Restore"),
                         uiManager);
        initToggleAction(enableDictionaryAction_, config_.enableDictionary, "ngosen-dictionary", "accessories-dictionary", _("Custom Dictionary"), N_("Custom Dictionary"),
                         uiManager);

        settingsAction_ = std::make_unique<SimpleAction>();
        settingsAction_->setShortText(_("Settings"));
        settingsAction_->setIcon("configure");
        connections_.emplace_back(settingsAction_->connect<SimpleAction::Activated>([](InputContext*) { startProcess({NGOSEN_SETTINGS_PATH}); }));
        uiManager.registerAction("ngosen-settings", settingsAction_.get());

        saveLogAction_ = std::make_unique<SimpleAction>();
        saveLogAction_->setShortText(_("Save typing log"));
        saveLogAction_->setIcon("document-save");
        connections_.emplace_back(saveLogAction_->connect<SimpleAction::Activated>([this](InputContext* ic) { saveTypingLog(ic); }));
        uiManager.registerAction("ngosen-save-log", saveLogAction_.get());
#ifdef NGOSEN_HAS_DBUS
        if (auto* dbusAddon = dbus()) {
            dbusObject_ = std::make_unique<NgoSenDBusObject>(this);
            if (!dbusAddon->call<IDBusModule::bus>()->addObjectVTable(NgoSenDBusObject::Path, NgoSenDBusObject::Interface, *dbusObject_)) {
                NGOSEN_WARN(std::string("Could not register ") + NgoSenDBusObject::Path + " on D-Bus; ngosen-save-log will not work");
                dbusObject_.reset();
            }
            gnomeShell_ = std::make_unique<ngosen::GnomeShellWatcher>(dbusAddon->call<IDBusModule::bus>(), [this](const std::string& app) { shellFocusedAppChanged(app); });
        }
#endif

        const std::string configDir = userConfigDir();

        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        reloadConfig();
        realMode = globalMode();
        instance_->inputContextManager().registerProperty("NgoSenState", &factory_);
        appRulesPath_ = configDir + "/ngosen-app-rules.conf";
        loadAppRules();
        menuEntries_ = {{inputMethodAction_.get(), &config_.showMenuInputMethod},
                        {charsetAction_.get(), &config_.showMenuCharset},
                        {spellCheckAction_.get(), &config_.showMenuSpellCheck},
                        {macroAction_.get(), &config_.showMenuMacro},
                        {capitalizeMacroAction_.get(), &config_.showMenuCapitalizeMacro},
                        {autoCapitalizeAction_.get(), &config_.showMenuAutoCapitalize},
                        {autoNonVnRestoreAction_.get(), &config_.showMenuAutoRestore},
                        {enableDictionaryAction_.get(), &config_.showMenuDictionary},
                        {saveLogAction_.get(), nullptr},
                        {settingsAction_.get(), nullptr}};
    }

    void NgoSenEngine::initToggleAction(std::unique_ptr<SimpleAction>& action, Option<bool>& option, const std::string& actionId, const std::string& iconName,
                                        const std::string& textLong, const char* textOnOff, UserInterfaceManager& uiManager) {
        action = std::make_unique<SimpleAction>();
        action->setShortText(textLong);
        action->setIcon(iconName);
        action->setCheckable(false);
        connections_.emplace_back(action->connect<SimpleAction::Activated>([this, &action, &option, textOnOff](InputContext* ic) {
            option.setValue(!option.value());
            syncOptions();
            saveConfig();
            refreshOption();
            updateAction(ic, action, option, _(textOnOff));
        }));
        uiManager.registerAction(actionId, action.get());
    }

    void NgoSenEngine::updateAction(InputContext* ic, std::unique_ptr<SimpleAction>& action, Option<bool>& option, const std::string& textOnOff) {
        action->setShortText((option.value() ? "✔ " : "✖ ") + textOnOff);
        if (ic != nullptr) {
            action->update(ic);
        }
    }

    const bool* NgoSenEngine::shellDropsKeys() const {
#ifdef NGOSEN_HAS_DBUS
        if (gnomeShell_)
            return gnomeShell_->dropsKeys();
#endif
        return nullptr;
    }

    NgoSenEngine::~NgoSenEngine() {
        stop_flag_monitor.store(true, std::memory_order_release);
        if (mouse_thread.joinable()) {
            mouse_thread.join();
        }
        NGOSEN_INFO("Engine destroyed.");
    }

    std::vector<ngosen::KeymapEntry> NgoSenEngine::customKeymap() const {
        std::vector<ngosen::KeymapEntry> entries;
        if (config_.enableCustomKeymap.value()) {
            for (const auto& keymap : *customKeymap_.customKeymap) {
                entries.push_back({*keymap.key, *keymap.value});
            }
        }
        return entries;
    }

    void NgoSenEngine::reloadConfig() {
        RawConfig raw;
        readAsIni(raw, "conf/ngosen.conf");
        migrateLegacyMode(raw);
        const bool        saved    = std::filesystem::exists(userConfigDir() + "/ngosen.conf");
        const bool        upgraded = ngosen::upgradeSettings(raw, saved);
        const std::string release  = ngosen::releaseToAnnounce(raw, saved, NGOSEN_VERSION);
        config_.load(raw);
        if (upgraded || !release.empty())
            saveConfig();
        if (!release.empty())
            announceRelease(release);
        readAsIni(customKeymap_, CustomKeymapFile);
        readAsIni(macroTables_, MacroTableFile);
        macroTableObject_.reset(newMacroTable(macroTables_));
        readAsIni(appSwitchesTables_, AppSwitchFile);
        if (config_.enableDictionary.value()) {
#if NGOSEN_USE_MODERN_FCITX_API
            auto fd = StandardPaths::global().open(StandardPathsType::PkgData, "ngosen/vietnamese.cm.dict");
#else
            auto fd = StandardPath::global().open(StandardPath::Type::PkgData, "ngosen/vietnamese.cm.dict", O_RDONLY);
#endif
            if (fd.isValid()) {
                dictionary_.reset(NewDictionary(fd.release()));
            }
        } else {
#if NGOSEN_USE_MODERN_FCITX_API
            auto paths = StandardPaths::global().locateAll(StandardPathsType::PkgData, "ngosen/vietnamese.cm.dict");
#else
            auto paths = StandardPath::global().locateAll(StandardPath::Type::PkgData, "ngosen/vietnamese.cm.dict");
#endif
            for (const auto& p : paths) {
#if NGOSEN_USE_MODERN_FCITX_API
                if (!isInHomeDirectory(p.string())) {
                    auto fd = fcitx::UnixFD(::open(p.c_str(), O_RDONLY));
                    if (fd.isValid()) {
                        dictionary_.reset(NewDictionary(fd.release()));
#else
                if (!isInHomeDirectory(p)) {
                    int fd = ::open(p.c_str(), O_RDONLY);
                    if (fd != -1) {
                        dictionary_.reset(NewDictionary(fd));
#endif
                        break;
                    }
                }
            }
        }
        loadAppRules();
        populateConfig();
    }

    void NgoSenEngine::announceRelease(const std::string& release) {
#ifdef NGOSEN_HAS_NOTIFICATIONS
        NGOSEN_INFO("Announcing release " + release);
        // fcitx5 starts with the session; a notification sent before the desktop is up is lost.
        constexpr uint64_t delayUs = 5000000;
        announceTimer_             = instance_->eventLoop().addTimeEvent(CLOCK_MONOTONIC, ::fcitx::now(CLOCK_MONOTONIC) + delayUs, 0, [this, release](EventSourceTime*, uint64_t) {
            auto* addon = notifications();
            if (addon == nullptr) {
                NGOSEN_WARN("No notifications addon; release " + release + " is not announced");
                return false;
            }
            // It is shown only once, and Plasma keeps no history of an expired one, so it waits for the user.
            // The text names the release, so it changes with each feature release.
            constexpr int32_t neverExpire = 0;
            addon->call<INotifications::sendNotification>(
                "fcitx", 0, "fcitx-ngosen", _("What's new in Ngó Sen?"),
                _("Version 1.7: the settings window also comes in a GTK edition that looks like a GNOME app. See also Cứu chữ (retype words typed with Vietnamese off), Cứu dấu "
                  "(select a word and type a tone mark to change it), the English/Vietnamese key and typing tips."),
                std::vector<std::string>{"open", _("Show")}, neverExpire, [](const std::string&) { startProcess({NGOSEN_SETTINGS_PATH, "--page", "whats-new"}); }, [](uint32_t) {});
            return false;
        });
#endif
    }

    const Configuration* NgoSenEngine::getSubConfig(const std::string& path) const {
        if (path == "custom_keymap")
            return &customKeymap_;
        if (path == "ngosen-macro") {
            return &macroTables_;
        }
        if (path == "app_rules") {
            return &appRulesTables_;
        }
        if (path == "app_switches")
            return &appSwitchesTables_;
        return nullptr;
    }

    void NgoSenEngine::setConfig(const RawConfig& config) {
        RawConfig migrated = config;
        migrateLegacyMode(migrated);
        config_.load(migrated, true);
        saveConfig();
        populateConfig();
    }

    void NgoSenEngine::syncOptions() {
        auto& o                            = options_;
        o.inputMethod                      = config_.inputMethod.value();
        o.outputCharset                    = config_.outputCharset.value();
        o.spellCheck                       = config_.spellCheck.value();
        o.modernStyle                      = config_.modernStyle.value();
        o.freeMarking                      = config_.freeMarking.value();
        o.w2u                              = static_cast<int>(config_.w2u.value());
        o.bracketTransform                 = static_cast<int>(config_.bracketTransform.value());
        o.timeFormat                       = config_.timeFormat.value();
        o.dateFormat                       = config_.dateFormat.value();
        o.autoNonVnRestore                 = config_.autoNonVnRestore.value();
        o.ddFreeStyle                      = config_.ddFreeStyle.value();
        o.backspaceRemovesTone             = config_.backspaceRemovesTone.value();
        o.enableMacro                      = config_.enableMacro.value();
        o.enableMacroInOffMode             = config_.enableMacroInOffMode.value();
        o.capitalizeMacro                  = config_.capitalizeMacro.value();
        o.autoCapitalizeAfterPunctuation   = config_.autoCapitalizeAfterPunctuation.value();
        o.capitalizeStartOfText            = config_.capitalizeStartOfText.value();
        o.doubleSpaceToPeriod              = config_.doubleSpaceToPeriod.value();
        o.doubleHyphenToEmDash             = config_.doubleHyphenToEmDash.value();
        o.retypeAfterSwitch                = config_.retypeAfterSwitch.value();
        o.retoneSelection                  = config_.retoneSelection.value();
        o.messengerSelectOvertype          = config_.messengerSelectOvertype.value();
        o.waitSurroundingEvent             = config_.waitSurroundingEvent.value();
        o.waitSurroundingMinPerKeyMs       = config_.waitSurroundingMinPerKeyMs.value();
        o.waitSurroundingTimeoutMs         = config_.waitSurroundingTimeoutMs.value();
        o.waitSurroundingShortMs           = config_.waitSurroundingShortMs.value();
        o.waitSurroundingSettleMs          = config_.waitSurroundingSettleMs.value();
        o.waitSurroundingSettleFirstWordMs = config_.waitSurroundingSettleFirstWordMs.value();
        o.waitSurroundingProbeEvery        = config_.waitSurroundingProbeEvery.value();
        o.surrDeleteSleepMs                = config_.surrDeleteSleepMs.value();
        o.surrCommitSleepMs                = config_.surrCommitSleepMs.value();
        switch (config_.lockWordModifier.value()) {
            case LockWordModifier::Shift: o.lockWordKey = ngosen::LockWordKey::Shift; break;
            case LockWordModifier::Ctrl: o.lockWordKey = ngosen::LockWordKey::Ctrl; break;
            case LockWordModifier::Alt: o.lockWordKey = ngosen::LockWordKey::Alt; break;
            case LockWordModifier::Disabled:
            default: o.lockWordKey = ngosen::LockWordKey::None; break;
        }
    }

    void NgoSenEngine::populateConfig() {
        ngosen::setUiVietnamese(*config_.uiLanguage == UiLanguage::Vietnamese);
        syncOptions();
        refreshEngine();
        refreshOption();
        updateInputMethodAction(nullptr);
        updateCharsetAction(nullptr);
        updateAction(nullptr, spellCheckAction_, config_.spellCheck, _("Spell Check"));
        updateAction(nullptr, macroAction_, config_.enableMacro, _("Macro"));
        updateAction(nullptr, capitalizeMacroAction_, config_.capitalizeMacro, _("Capitalize Macro"));
        updateAction(nullptr, capitalizeSentencesAction_, config_.autoCapitalizeAfterPunctuation, _("After . ! ? and Enter"));
        updateAction(nullptr, capitalizeStartAction_, config_.capitalizeStartOfText, _("Start of text or line"));
        updateAction(nullptr, autoNonVnRestoreAction_, config_.autoNonVnRestore, _("Auto Non-VN Restore"));
        updateAction(nullptr, enableDictionaryAction_, config_.enableDictionary, _("Custom Dictionary"));
        inputMethodAction_->setShortText(_("Input Method"));
        charsetAction_->setShortText(_("Charset"));
        autoCapitalizeAction_->setShortText(_("Auto Capitalize"));
        settingsAction_->setShortText(_("Settings"));
        saveLogAction_->setShortText(_("Save typing log"));
        if (!factory_.registered())
            return;
        // The tray keeps showing the last field's menu while the settings window has focus.
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            const auto shown = ic->statusArea().actions(StatusGroup::InputMethod);
            if (std::find(shown.begin(), shown.end(), settingsAction_.get()) != shown.end())
                fillStatusArea(ic);
            return true;
        });
    }

    void NgoSenEngine::setSubConfig(const std::string& path, const RawConfig& config) {
        if (path == "custom_keymap") {
            customKeymap_.load(config, true);
            safeSaveAsIni(customKeymap_, CustomKeymapFile);
            refreshEngine();
        } else if (path == "ngosen-macro") {
            macroTables_.load(config, true);
            safeSaveAsIni(macroTables_, MacroTableFile);
            macroTableObject_.reset(newMacroTable(macroTables_));
            refreshEngine();
        } else if (path == "app_rules") {
            appRulesTables_.load(config, true);
            {
                std::lock_guard<std::mutex> lock(appRulesMutex_);
                for (auto it = appRules_.begin(); it != appRules_.end();) {
                    if (!isStartsWith(it->first, "ctx_")) {
                        it = appRules_.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (const auto& rule : *appRulesTables_.rules) {
                    appRules_[*rule.app] = intToMode(*rule.mode);
                }
            }
            saveAppRules();
            refreshEngine();
        } else if (path == "app_switches") {
            appSwitchesTables_.load(config, true);
            // An empty list is Defaults, which forgets open windows without a name too.
            if (appSwitchesTables_.switches->empty())
                ctxSwitches_.clear();
            safeSaveAsIni(appSwitchesTables_, AppSwitchFile);
            refreshEngine();
        }
    }

    std::string NgoSenEngine::subMode(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        switch (realMode) {
            case NgoSenMode::Off: return _("OFF");
            case NgoSenMode::Emoji: return _("Emoji Picker");
            default: return *config_.inputMethod;
        }
    }

    void NgoSenEngine::activate(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        auto* ic = event.inputContext();
        if (dropStaleSurroundingText(ic))
            NGOSEN_INFO("Dropped stale surrounding text");
        static std::atomic<bool> mouseThreadStarted{false};
        if (!mouseThreadStarted.exchange(true))
            startMouseReset();

        if (ic->capabilityFlags().test(CapabilityFlag::Preedit))
            instance_->inputContextManager().setPreeditEnabledByDefault(true);

        std::string appName = getProgramName(ic);
        NGOSEN_INFO("App name: " + appName);

        // Next to other input methods fcitx5 owns the switch, so picking Ngó Sen there means Vietnamese.
        // The group is checked first: a lone input method can be activated with a plain InputContextEvent.
        if (event.type() == EventType::InputContextSwitchInputMethod && instance_->inputMethodManager().currentGroup().inputMethodList().size() > 1 &&
            switchPickedByUser(static_cast<InputContextSwitchInputMethodEvent&>(event).reason())) {
            // An Off picked from the menu stays, as it did before apps had switches.
            if (findAppRule(appName) != NgoSenMode::Off)
                setAppSwitch(appName, true);
            if (!*config_.vietnameseOn) {
                config_.vietnameseOn.setValue(true);
                saveConfig();
            }
        }

        const NgoSenMode targetMode = modeFor(appName);
        NGOSEN_INFO("Target mode: " + ngosen::ModeI18NAnnotation::toString(targetMode));

        updateInputMethodAction(event.inputContext());
        updateCharsetAction(event.inputContext());

        auto* state = stateFor(ic);
        state->activate(targetMode, event.type() == EventType::InputContextFocusIn);
        ic->updateUserInterface(UserInterfaceComponent::StatusArea);
        if (targetMode == NgoSenMode::Emoji) {
            state->updateEmojiPreedit();
        } else {
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            if (realMode == NgoSenMode::Preedit)
                ic->updatePreedit();
        }
        fillStatusArea(ic);
        state->offerRetype();
    }

    void NgoSenEngine::keyEvent(const InputMethodEntry& /*entry*/, KeyEvent& keyEvent) {
        auto* ic = keyEvent.inputContext();
        dropStaleSurroundingText(ic);

        if (isSelectingAppMode_ && g_mouse_clicked.load(std::memory_order_acquire)) {
            closeAppModeMenu();
            closeModeMenuPanel(ic, true);
        }

        if (handleToggleKey(keyEvent))
            return;

        if (isSelectingAppMode_) {
            handleModeMenuKey(keyEvent);
            return;
        }

        if (!keyEvent.isRelease() && !config_.cycleModeKey->empty() && keyEvent.key().checkKeyList(*config_.cycleModeKey)) {
            cycleMode(ic);
            keyEvent.filterAndAccept();
            return;
        }

        // In English the menu key types its character; ` is common in code and Markdown.
        if (realMode != NgoSenMode::Off && !keyEvent.isRelease() && !config_.modeMenuKey->empty() && keyEvent.key().checkKeyList(*config_.modeMenuKey)) {
            openModeMenu(ic);
            keyEvent.filterAndAccept();
            return;
        }
        auto*                 state = stateFor(keyEvent.inputContext());
        ngosen::FcitxKeyPress press(keyEvent);
        state->keyEvent(press);
    }

    void NgoSenEngine::handleModeMenuKey(KeyEvent& keyEvent) {
        if (keyEvent.isRelease())
            return;
        auto*  ic       = keyEvent.inputContext();
        auto   menuList = std::dynamic_pointer_cast<CommonCandidateList>(ic->inputPanel().candidateList());
        KeySym keySym   = keyEvent.key().sym();
        keyEvent.filterAndAccept();

        switch (keySym) {
            case FcitxKey_Tab:
            case FcitxKey_Down: moveModeMenuCursor(ic, menuList.get(), 1); return;
            case FcitxKey_ISO_Left_Tab:
            case FcitxKey_Up: moveModeMenuCursor(ic, menuList.get(), -1); return;
            case FcitxKey_space:
            case FcitxKey_Return: {
                if (menuList && !menuList->empty()) {
                    int selectedIndex = menuList->globalCursorIndex();
                    if (selectedIndex < 0 || selectedIndex >= menuList->totalSize()) {
                        selectedIndex = 0;
                    }
                    menuList->candidateFromAll(selectedIndex).select(ic);
                }
                return;
            }
            case FcitxKey_Escape: closeModeMenuPanel(ic, false); return;
            default: break;
        }

        if (auto it = modeMenuMapping_.find(keySym); it != modeMenuMapping_.end()) {
            NGOSEN_INFO("Selected mode: " + ngosen::ModeI18NAnnotation::toString(it->second));
            pickMenuMode(ic, it->second, keySym == Key(*config_.shortcutDefault).sym());
            return;
        }
        const auto& kl = *config_.modeMenuKey;
        if (kl.size() != 1 || kl[0].hasModifier())
            return;
        std::string charStr = Key::keySymToUTF8(kl[0].sym());
        if (!charStr.empty() && keySym == typeKeyForModeMenuHotkey(kl[0].sym(), config_)) {
            closeModeMenuPanel(ic, true);
            ic->commitString(charStr);
        }
    }

    void NgoSenEngine::moveModeMenuCursor(InputContext* ic, CommonCandidateList* menuList, int delta) {
        if (!menuList || menuList->empty()) {
            return;
        }

        int totalSize = menuList->totalSize();
        if (totalSize <= 1) {
            return;
        }

        int cursorIndex = menuList->globalCursorIndex();
        if (cursorIndex < 0 || cursorIndex >= totalSize) {
            cursorIndex = 0;
        }

        int nextIndex = cursorIndex + delta;
        if (nextIndex < 0) {
            nextIndex = totalSize - 1;
        } else if (nextIndex >= totalSize) {
            nextIndex = 0;
        }

        menuList->setGlobalCursorIndex(nextIndex);
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void NgoSenEngine::closeModeMenuPanel(InputContext* ic, bool resetState) {
        isSelectingAppMode_ = false;
        ic->inputPanel().reset();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        if (resetState) {
            auto* state = stateFor(ic);
            state->commitBuffer();
            state->reset();
        }
    }

    void NgoSenEngine::pickMenuMode(InputContext* ic, NgoSenMode mode, bool isDefault) {
        if (mode != NgoSenMode::Emoji) {
            if (isDefault) {
                clearAppRule(currentConfigureApp_);
            } else {
                setAppRule(currentConfigureApp_, mode);
                if (!isStartsWith(currentConfigureApp_, "ctx_")) {
                    saveAppRules();
                }
            }
            // A mode picked for the app wins over where the toggle key last left it.
            clearAppSwitch(currentConfigureApp_);
        }

        closeModeMenuPanel(ic, true);
        setMode(mode, ic);
        if (mode == NgoSenMode::Emoji) {
            stateFor(ic)->updateEmojiPreedit();
        } else {
            showCycleModeNotification(mode, ic);
        }
    }

    std::vector<NgoSenMode> NgoSenEngine::cycleModes(const std::string& appName) {
        auto                                      order      = stringutils::split(*config_.modeOrder, ",");
        std::vector<std::pair<std::string, bool>> visibility = {{"Sen", *config_.showModeSen},
                                                                {"Preedit", *config_.showModePreedit},
                                                                {"Emoji", *config_.showModeEmoji},
                                                                {"Off", *config_.showModeOff},
                                                                {"Default", *config_.showModeDefault}};

        std::vector<NgoSenMode>                   enabledModes;
        for (const auto& name : order) {
            bool visible = false;
            for (const auto& v : visibility) {
                if (v.first == name) {
                    visible = v.second;
                    break;
                }
            }
            if (!visible)
                continue;
            std::optional<NgoSenMode> mode = std::nullopt;
            if (name == "Sen")
                mode = NgoSenMode::Sen;
            else if (name == "Preedit")
                mode = NgoSenMode::Preedit;
            else if (name == "Emoji")
                mode = NgoSenMode::Emoji;
            else if (name == "Off")
                mode = NgoSenMode::Off;
            else if (name == "Default")
                mode = defaultModeFor(appName);
            else
                continue;

            if (std::find(enabledModes.begin(), enabledModes.end(), mode.value()) == enabledModes.end()) {
                enabledModes.push_back(mode.value());
            }
        }
        return enabledModes;
    }

    void NgoSenEngine::cycleMode(InputContext* ic) {
        NGOSEN_INFO("Cycle mode key pressed");
        const std::string appName      = getProgramName(ic);
        const NgoSenMode  current      = modeFor(appName);
        const auto        enabledModes = cycleModes(appName);
        if (enabledModes.empty())
            return;

        auto       it       = std::find(enabledModes.begin(), enabledModes.end(), current);
        NgoSenMode nextMode = it == enabledModes.end() ? enabledModes[0] : enabledModes[(static_cast<size_t>(it - enabledModes.begin()) + 1) % enabledModes.size()];
        setMode(nextMode, ic);
        setAppRule(appName, nextMode);
        clearAppSwitch(appName);
        showCycleModeNotification(nextMode, ic);
    }

    void NgoSenEngine::openModeMenu(InputContext* ic) {
        NGOSEN_INFO("Mode menu key pressed");
        auto* state = stateFor(ic);
        if (state != nullptr) {
            state->commitBuffer();
            state->reset();
        }
        currentConfigureApp_ = getProgramName(ic);
        g_mouse_clicked.store(false, std::memory_order_release);
        setMode(modeFor(currentConfigureApp_), ic);
        showAppModeMenu(ic);
    }

    void NgoSenEngine::reset(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        NGOSEN_INFO("Reset engine");
        auto* state = stateFor(event.inputContext());
        if (!state->isEmptyHistory() && event.type() != EventType::InputContextFocusOut) {
            return;
        }

        if (event.type() == EventType::InputContextFocusOut || event.type() == EventType::InputContextReset) {
            state->reset(event.type() == EventType::InputContextFocusOut);
        }
    }

    void NgoSenEngine::deactivate(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        stateFor(event.inputContext())->deactivate(event.type() == EventType::InputContextFocusOut);
    }

    void NgoSenEngine::refreshEngine() {
        if (!factory_.registered())
            return;
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            auto* state = stateFor(ic);
            state->setEngine();
            if (ic->hasFocus()) {
                // Re-resolve the focused window's rule; setEngine() must not
                // reset it to the global mode.
                setMode(modeFor(getProgramName(ic)), ic);
                state->reset();
            }
            return true;
        });
    }

    void NgoSenEngine::refreshOption() {
        if (!factory_.registered())
            return;
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            auto* state = stateFor(ic);
            state->setOption();
            if (ic->hasFocus())
                state->reset();
            return true;
        });
    }

    void NgoSenEngine::initChoiceMenu(std::unique_ptr<SimpleAction>& action, std::vector<std::unique_ptr<SimpleAction>>& choices, std::unique_ptr<Menu>& menu,
                                      const std::string& actionId, const std::string& text, const std::string& iconName, const std::vector<std::string>& names,
                                      const std::function<void(InputContext*, const std::string&)>& pick, UserInterfaceManager& uiManager) {
        action = std::make_unique<SimpleAction>();
        action->setShortText(text);
        action->setIcon(iconName);
        uiManager.registerAction(actionId, action.get());
        menu = std::make_unique<Menu>();
        action->setMenu(menu.get());
        for (const auto& name : names) {
            choices.emplace_back(std::make_unique<SimpleAction>());
            auto* choice = choices.back().get();
            choice->setShortText(name);
            choice->setCheckable(true);
            uiManager.registerAction(stringutils::concat(actionId, "-", name), choice);
            connections_.emplace_back(choice->connect<SimpleAction::Activated>([pick, name](InputContext* ic) { pick(ic, name); }));
            menu->addAction(choice);
        }
    }

    void NgoSenEngine::checkChoice(const std::vector<std::unique_ptr<SimpleAction>>& choices, const std::string& actionId, const std::string& current, InputContext* ic) {
        const auto name = stringutils::concat(actionId, "-", current);
        for (const auto& action : choices) {
            action->setChecked(action->name() == name);
            if (ic != nullptr)
                action->update(ic);
        }
    }

    void NgoSenEngine::updateCharsetAction(InputContext* ic) {
        checkChoice(charsetSubAction_, CharsetActionId, *config_.outputCharset, ic);
    }

    void NgoSenEngine::updateInputMethodAction(InputContext* ic) {
        checkChoice(inputMethodSubAction_, InputMethodActionId, *config_.inputMethod, ic);
    }

    void NgoSenEngine::fillStatusArea(InputContext* ic) {
        auto& statusArea = ic->statusArea();
        statusArea.clearGroup(StatusGroup::InputMethod);
        for (const auto& entry : menuEntries_) {
            if (entry.shown == nullptr || entry.shown->value())
                statusArea.addAction(StatusGroup::InputMethod, entry.action);
        }
        ic->updateUserInterface(UserInterfaceComponent::StatusArea);
    }

    void NgoSenEngine::loadAppRules() {
        {
            std::lock_guard<std::mutex>                 lock(appRulesMutex_);
            std::unordered_map<std::string, NgoSenMode> ctxRules;
            for (const auto& [app, mode] : appRules_) {
                if (isStartsWith(app, "ctx_")) {
                    ctxRules[app] = mode;
                }
            }
            appRules_ = std::move(ctxRules);
        }
        auto loadFromFile = [this](const std::string& path) {
            if (path.empty()) {
                NGOSEN_WARN("App rules path is empty, skipping load");
                return;
            }
            std::ifstream file(path);
            if (!file.is_open())
                return;

            std::unordered_map<std::string, NgoSenMode> tempRules;
            std::string                                 line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#')
                    continue;
                auto delimiterPos = line.find('=');
                if (delimiterPos != std::string::npos) {
                    std::string app  = line.substr(0, delimiterPos);
                    std::string mode = line.substr(delimiterPos + 1);
                    // Older versions saved this for every app on GNOME; see getProgramName.
                    if (app == "gnome-shell")
                        continue;
                    try {
                        tempRules[app] = intToMode(std::stoi(mode));
                    } catch (const std::exception&) { NGOSEN_WARN("Invalid mode value for app: " + app); }
                }
            }
            file.close();

            std::lock_guard<std::mutex> lock(appRulesMutex_);
            for (const auto& [app, mode] : tempRules) {
                appRules_[app] = mode;
            }
        };
        loadFromFile(appRulesPath_);

        std::lock_guard<std::mutex> lock(appRulesMutex_);
        std::vector<ngosenAppRule>  rules;
        for (const auto& pair : appRules_) {
            if (pair.first.find("ctx_") == 0)
                continue;
            ngosenAppRule rule;
            rule.app.setValue(pair.first);
            rule.mode.setValue(modeToInt(pair.second));
            rules.push_back(std::move(rule));
        }
        appRulesTables_.rules.setValue(std::move(rules));
    }

    void NgoSenEngine::saveAppRules() const {
        // Method is const but locks mutable appRulesMutex_ to safely read appRules_ state
        std::ofstream file(appRulesPath_, std::ios::trunc);
        if (!file.is_open())
            return;

        file << "# Ngó Sen Per-App Configuration\n";
        file << "# 0 = Off, 2 = Sen (1, 3, 4 and 8 are read as Sen too), 5 = Preedit, 6 = Emoji Picker\n";
        std::lock_guard<std::mutex> lock(appRulesMutex_);
        for (const auto& pair : appRules_) {
            bool currentIsCtx = isStartsWith(pair.first, "ctx_");
            if (!currentIsCtx) {
                file << pair.first << "=" << modeToInt(pair.second) << "\n";
            }
        }
        file.close();
    }

    NgoSenMode NgoSenEngine::modeFor(const std::string& appName) const {
        const auto rule = findAppRule(appName);
        // Until the toggle key is pressed in the app, its rule says whether it types Vietnamese.
        const bool on = findAppSwitch(appName).value_or(rule ? *rule != NgoSenMode::Off : *config_.vietnameseOn);
        if (!on)
            return NgoSenMode::Off;
        if (rule && *rule != NgoSenMode::Off)
            return *rule;
        return vietnameseModeFor(appName);
    }

    bool switchPickedByUser(InputMethodSwitchedReason reason) {
        using Reason = InputMethodSwitchedReason;
        return reason == Reason::Trigger || reason == Reason::AltTrigger || reason == Reason::Enumerate || reason == Reason::Activate || reason == Reason::Other;
    }

    NgoSenMode NgoSenEngine::defaultModeFor(const std::string& appName) const {
        return *config_.vietnameseOn ? vietnameseModeFor(appName) : NgoSenMode::Off;
    }

    NgoSenMode NgoSenEngine::vietnameseModeFor(const std::string& appName) const {
        const NgoSenMode mode = vietnameseMode();
        return mode == NgoSenMode::Sen && typesInPreeditByDefault(appName) ? NgoSenMode::Preedit : mode;
    }

    std::optional<std::string> NgoSenEngine::appRuleName(const std::string& appName) const {
        std::lock_guard<std::mutex> lock(appRulesMutex_);
        // Exact match wins so a "foo.desktop" rule can still override "foo".
        for (const auto& name : {appName, stripDesktopSuffix(appName)}) {
            if (appRules_.count(name) > 0)
                return name;
        }
        return std::nullopt;
    }

    std::optional<NgoSenMode> NgoSenEngine::findAppRule(const std::string& appName) const {
        const auto name = appRuleName(appName);
        if (!name)
            return std::nullopt;
        std::lock_guard<std::mutex> lock(appRulesMutex_);
        const auto                  it = appRules_.find(*name);
        return it == appRules_.end() ? std::nullopt : std::optional<NgoSenMode>(it->second);
    }

    NgoSenMode NgoSenEngine::vietnameseMode() const {
        return config_.mode.value() == NgoSenMode::Off ? NgoSenMode::Sen : config_.mode.value();
    }

    NgoSenMode NgoSenEngine::globalMode() const {
        return *config_.vietnameseOn ? vietnameseMode() : NgoSenMode::Off;
    }

    bool NgoSenEngine::isToggleKey(const Key& key) const {
        return !config_.toggleKey->empty() && key.checkKeyList(*config_.toggleKey);
    }

    bool NgoSenEngine::handleToggleKey(KeyEvent& keyEvent) {
        const Key& key = keyEvent.key();
        if (keyEvent.isRelease()) {
            if (!key.isModifier())
                return false;
            // Wait for the last one, or the retype's XTEST backspaces reach the app as Ctrl+BackSpace.
            if (otherModifierHeld(key))
                return false;
            if (retypeOnRelease_) {
                retypeOnRelease_ = false;
                stateFor(keyEvent.inputContext())->offerRetype();
            }
            if (!toggleTapArmed_)
                return false;
            toggleTapArmed_ = false;
#if NGOSEN_USE_MODERN_FCITX_API
            // A long hold, such as Ctrl+Shift+click, is not a tap.
            if (!instance_->globalConfig().checkModifierOnlyKeyTimeout(toggleTapTime_))
                return false;
#endif
            toggleVietnamese(keyEvent.inputContext());
            return true;
        }
        if (!isToggleKey(key)) {
            toggleTapArmed_  = false;
            retypeOnRelease_ = false;
            return false;
        }
        // A modifier-only key such as Ctrl+Shift would otherwise fire inside Ctrl+Shift+T.
        if (key.isModifier()) {
            // A held key repeats; the hold counts from the first press.
            if (!toggleTapArmed_)
                toggleTapTime_ = now(CLOCK_MONOTONIC);
            toggleTapArmed_ = true;
            return false;
        }
        // Ctrl+Space: offer the retype once Ctrl is up too.
        retypeOnRelease_ = realMode == NgoSenMode::Off && otherModifierHeld(key);
        toggleVietnamese(keyEvent.inputContext(), !retypeOnRelease_);
        keyEvent.filterAndAccept();
        return true;
    }

    void NgoSenEngine::toggleVietnamese(InputContext* ic, bool offerRetype) {
        if (isSelectingAppMode_) {
            closeAppModeMenu();
            closeModeMenuPanel(ic, true);
        } else if (auto* state = stateFor(ic)) {
            // Finish the word being typed in the mode it was typed in.
            state->commitBuffer();
            state->reset();
        }
        const bool        turnOn = realMode == NgoSenMode::Off;
        const std::string app    = getProgramName(ic);
        setAppSwitch(app, turnOn);
        // Apps never switched in follow the last switch.
        config_.vietnameseOn.setValue(turnOn);
        saveConfig();
        const NgoSenMode mode = modeFor(app);
        NGOSEN_INFO(std::string("Toggle key: ") + (turnOn ? "Vietnamese" : "English") + " in " + app);
        setMode(mode, ic);
#if NGOSEN_USE_MODERN_FCITX_API
        instance_->showCustomInputMethodInformation(ic, turnOn ? "vi" : "en");
#else
        // Shows "Ngó Sen (OFF)" or the input method; older fcitx5 has no custom text here.
        instance_->showInputMethodInformation(ic);
#endif
        if (turnOn && offerRetype)
            stateFor(ic)->offerRetype();
    }

    void NgoSenEngine::setAppRule(const std::string& appName, NgoSenMode mode) {
        auto rules = *appRulesTables_.rules;

        bool found = false;
        for (auto& rule : rules) {
            if (*rule.app == appName) {
                rule.mode.setValue(modeToInt(mode));
                found = true;
                break;
            }
        }

        if (!found) {
            ngosenAppRule newRule;
            newRule.app.setValue(appName);
            newRule.mode.setValue(modeToInt(mode));
            rules.push_back(std::move(newRule));
        }

        {
            std::lock_guard<std::mutex> lock(appRulesMutex_);
            appRules_[appName] = mode;
        }
        appRulesTables_.rules.setValue(std::move(rules));
    }

    std::optional<std::string> NgoSenEngine::appSwitchName(const std::string& appName) const {
        if (isStartsWith(appName, "ctx_"))
            return ctxSwitches_.count(appName) > 0 ? std::optional<std::string>(appName) : std::nullopt;
        // Exact match wins so a "foo.desktop" switch can differ from "foo".
        for (const auto& name : {appName, stripDesktopSuffix(appName)}) {
            for (const auto& entry : *appSwitchesTables_.switches) {
                if (*entry.app == name)
                    return name;
            }
        }
        return std::nullopt;
    }

    std::optional<bool> NgoSenEngine::findAppSwitch(const std::string& appName) const {
        const auto name = appSwitchName(appName);
        if (!name)
            return std::nullopt;
        if (isStartsWith(*name, "ctx_"))
            return ctxSwitches_.at(*name);
        for (const auto& entry : *appSwitchesTables_.switches) {
            if (*entry.app == *name)
                return *entry.vietnamese;
        }
        return std::nullopt;
    }

    void NgoSenEngine::setAppSwitch(const std::string& appName, bool vietnamese) {
        if (isStartsWith(appName, "ctx_")) {
            ctxSwitches_[appName] = vietnamese;
            return;
        }
        auto switches = *appSwitchesTables_.switches;
        auto it       = std::find_if(switches.begin(), switches.end(), [&appName](const auto& entry) { return *entry.app == appName; });
        if (it == switches.end()) {
            ngosenAppSwitch entry;
            entry.app.setValue(appName);
            entry.vietnamese.setValue(vietnamese);
            switches.push_back(std::move(entry));
        } else if (*it->vietnamese == vietnamese) {
            return;
        } else {
            it->vietnamese.setValue(vietnamese);
        }
        appSwitchesTables_.switches.setValue(std::move(switches));
        safeSaveAsIni(appSwitchesTables_, AppSwitchFile);
    }

    void NgoSenEngine::clearAppSwitch(const std::string& appName) {
        if (isStartsWith(appName, "ctx_")) {
            ctxSwitches_.erase(appName);
            return;
        }
        if (!appSwitchName(appName))
            return;
        // Both spellings, or the one left over would still apply to appName.
        const std::string bareName = stripDesktopSuffix(appName);
        auto              switches = *appSwitchesTables_.switches;
        switches.erase(std::remove_if(switches.begin(), switches.end(), [&](const auto& entry) { return *entry.app == appName || *entry.app == bareName; }), switches.end());
        appSwitchesTables_.switches.setValue(std::move(switches));
        safeSaveAsIni(appSwitchesTables_, AppSwitchFile);
    }

    void NgoSenEngine::closeAppModeMenu() {
        isSelectingAppMode_ = false;
        g_mouse_clicked.store(false, std::memory_order_release);
    }

    std::vector<NgoSenEngine::ModeMenuItem> NgoSenEngine::modeMenuItems() {
        auto                                          getShortcut = [](const std::string& shortcut) { return Key(shortcut).sym(); };

        std::unordered_map<std::string, ModeMenuItem> modeMap = {
            {"Sen", {NgoSenMode::Sen, _("Sen"), getShortcut(*config_.shortcutSen), *config_.showModeSen}},
            {"Preedit", {NgoSenMode::Preedit, _("Preedit"), getShortcut(*config_.shortcutPreedit), *config_.showModePreedit}},
            {"Emoji", {NgoSenMode::Emoji, _("Emoji Picker"), getShortcut(*config_.shortcutEmoji), *config_.showModeEmoji}},
            {"Off", {NgoSenMode::Off, _("OFF"), getShortcut(*config_.shortcutOff), *config_.showModeOff}},
            {"Default", {defaultModeFor(currentConfigureApp_), _("Default Typing"), getShortcut(*config_.shortcutDefault), *config_.showModeDefault}}};

        std::vector<ModeMenuItem> allModes;
        auto                      order = stringutils::split(*config_.modeOrder, ",");
        for (const auto& name : order) {
            auto it = modeMap.find(name);
            if (it != modeMap.end()) {
                allModes.push_back(it->second);
            }
        }

        // Fallback for missing modes
        for (const auto& [name, info] : modeMap) {
            if (std::find(order.begin(), order.end(), name) == order.end()) {
                allModes.push_back(info);
            }
        }
        return allModes;
    }

    void NgoSenEngine::showAppModeMenu(InputContext* ic) {
        isSelectingAppMode_ = true;

        auto candidateList = std::make_unique<CommonCandidateList>();

        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(10);

        auto getLabel = [&](const NgoSenMode& modeName, const std::string& modeLabel) {
            if (modeName == realMode) {
                return Text(">> " + modeLabel);
            }
            return Text("   " + modeLabel);
        };

        int                        activeSelectionIdx  = -1;
        int                        currentCandidateIdx = 0;
        std::unordered_set<KeySym> usedModeKeys;

        modeMenuMapping_.clear();

        for (const auto& info : modeMenuItems()) {
            if (!info.visible)
                continue;
            const bool hasShortcut = info.key != FcitxKey_None && info.key != FcitxKey_VoidSymbol;
            if (hasShortcut && usedModeKeys.insert(info.key).second) {
                modeMenuMapping_[info.key] = info.mode;
            }

            const bool        isDefaultItem = (info.label == _("Default Typing"));
            const std::string keyUtf8       = Key::keySymToUTF8(info.key);
            std::string       keyLabel      = keyUtf8.empty() ? "" : "[" + keyUtf8 + "] ";
            candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(info.mode, keyLabel + info.label),
                                                                         [this, mode = info.mode, isDefaultItem](InputContext* ic) { pickMenuMode(ic, mode, isDefaultItem); }));

            if (info.mode == realMode && !isDefaultItem) {
                activeSelectionIdx = currentCandidateIdx;
            } else if (isDefaultItem && !findAppRule(currentConfigureApp_) && !findAppSwitch(currentConfigureApp_)) {
                activeSelectionIdx = currentCandidateIdx;
            }
            currentCandidateIdx++;
        }
        appendTypeHotkeyItem(*candidateList);

        if (activeSelectionIdx != -1) {
            candidateList->setGlobalCursorIndex(activeSelectionIdx);
        } else if (candidateList->totalSize() > 0) {
            candidateList->setGlobalCursorIndex(0);
        }

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->inputPanel().setAuxDown(Text(_("App: ") + currentConfigureApp_));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void NgoSenEngine::appendTypeHotkeyItem(CommonCandidateList& candidateList) {
        const auto& kl = *config_.modeMenuKey;
        if (kl.size() != 1 || kl[0].hasModifier())
            return;
        std::string charStr = Key::keySymToUTF8(kl[0].sym());
        if (charStr.empty())
            return;
        KeySym      typeKeySym   = typeKeyForModeMenuHotkey(kl[0].sym(), config_);
        std::string typeKeyLabel = Key::keySymToUTF8(typeKeySym);
        std::string label        = "[" + typeKeyLabel + "] " + _("Type") + " " + charStr;
        candidateList.append(std::make_unique<AppModeCandidateWord>(Text(label), [this, charStr](InputContext* ic) {
            closeModeMenuPanel(ic, true);
            ic->commitString(charStr);
        }));
    }

    void NgoSenEngine::showCycleModeNotification(NgoSenMode mode, InputContext* ic) {
        auto candidateList = std::make_unique<CommonCandidateList>();
        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(1);

        auto cleanup = [](InputContext* ic) {
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        };

        // Map mode to label
        std::string modeLabel;
        switch (mode) {
            case NgoSenMode::Sen: modeLabel = _("Sen"); break;
            case NgoSenMode::Preedit: modeLabel = _("Preedit"); break;
            case NgoSenMode::Emoji: modeLabel = _("Emoji Picker"); break;
            case NgoSenMode::Off: modeLabel = _("OFF"); break;
            default: modeLabel = _("Unknown Mode"); break;
        }

        auto setCurrentMode = [cleanup](NgoSenMode) { return [cleanup](InputContext* ic) { cleanup(ic); }; };

        candidateList->append(std::make_unique<AppModeCandidateWord>(Text("✓ " + modeLabel), setCurrentMode(mode)));

        candidateList->setGlobalCursorIndex(0);

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);

        clearPanelLater(ic, CYCLE_MODE_NOTIFICATION_TIMEOUT_USEC);
    }

    void NgoSenEngine::clearPanelLater(InputContext* ic, uint64_t delayUs) {
        cycleModeNotificationTimer_.reset();
        cycleModeNotificationTimer_ =
            instance_->eventLoop().addTimeEvent(CLOCK_MONOTONIC, ::fcitx::now(CLOCK_MONOTONIC) + delayUs, 0, [icRef = ic->watch()](EventSourceTime*, uint64_t) {
                if (auto* ic = icRef.get(); ic && ic->hasFocus()) {
                    ic->inputPanel().reset();
                    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                }
                return false;
            });
    }

    std::filesystem::path NgoSenEngine::typingLogDir() {
        const std::string stateHome = getEnv("XDG_STATE_HOME");
        const auto        dir       = stateHome.empty() ? std::filesystem::path(getEnv("HOME")) / ".local" / "state" : std::filesystem::path(stateHome);
        return dir / "ngosen";
    }

    std::filesystem::path NgoSenEngine::writeTypingLog() {
        const auto      dir = typingLogDir();
        std::error_code error;
        std::filesystem::create_directories(dir, error);
        // The log holds what the user typed, possibly passwords; keep it from other users.
        std::filesystem::permissions(dir, std::filesystem::perms::owner_all, error);

        char       stamp[32];
        const auto now = std::time(nullptr);
        std::tm    local{};
        localtime_r(&now, &local);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
        const auto        path = dir / (std::string("typing-") + stamp + ".log");
        const std::string text = std::string("# Ngó Sen ") + NGOSEN_VERSION + " typing log. It holds what you typed just before saving; read it before sharing.\n" +
            "# desktop=" + getEnv("XDG_CURRENT_DESKTOP") + " session=" + getEnv("XDG_SESSION_TYPE") + "\n" + recorder_.dump();

        UnixFD fd = UnixFD::own(open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600));
        if (!fd.isValid())
            return {};
        // A log saved earlier in the same second keeps its old mode otherwise.
        const bool written = fchmod(fd.fd(), 0600) == 0 && fs::safeWrite(fd.fd(), text.data(), text.size()) == static_cast<ssize_t>(text.size());
        return written && close(fd.release()) == 0 ? path : std::filesystem::path();
    }

    void NgoSenEngine::saveTypingLog(InputContext* ic) {
        const auto        path    = writeTypingLog();
        const std::string message = !path.empty() ? _("Typing log saved: ") + path.string() : _("Could not save the typing log to ") + typingLogDir().string();
        NGOSEN_INFO(message);
        if (ic == nullptr)
            return;
        ic->inputPanel().reset();
        ic->inputPanel().setAuxUp(Text(message));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        clearPanelLater(ic, 5000000);
    }

    void NgoSenEngine::setMode(NgoSenMode mode, InputContext* ic) {
        realMode = mode;
        if (ic != nullptr) {
            if (auto* state = stateFor(ic)) {
                state->clearAllBuffers();
            }
            ic->updateUserInterface(UserInterfaceComponent::StatusArea);
        }
    }

    std::string NgoSenEngine::subModeIconImpl(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        std::string baseIconName;
        switch (realMode) {
            case NgoSenMode::Off: baseIconName = "fcitx-ngosen-off"; break;
            case NgoSenMode::Emoji: baseIconName = "fcitx-ngosen-emoji"; break;
            default: baseIconName = "fcitx-ngosen"; break;
        }

        std::string iconName;
        if (*config_.useNgoSenIcons) {
            iconName = baseIconName;
        } else {
            const auto& iconTheme = config_.iconTheme.value();
            if (iconTheme == IconTheme::Light) {
                iconName = baseIconName + "-default-black";
            } else if (iconTheme == IconTheme::Dark) {
                iconName = baseIconName + "-default";
            } else {
                iconName = baseIconName + (isDarkMode() ? "-default" : "-default-black");
            }
        }

        static const bool kTrayWantsIconName = trayWantsIconName(getEnv("XDG_CURRENT_DESKTOP"), getEnv("DESKTOP_SESSION"));
        if (kTrayWantsIconName) {
            return iconName;
        }

        // Cache keyed on the resolved icon name — mode/theme changes
        // re-resolve automatically, no manual invalidation needed.
        if (iconCacheName_ == iconName && !iconCachePath_.empty()) {
            return iconCachePath_;
        }
        iconCacheName_ = iconName;

        // ── Default: resolve to absolute path (KDE, GNOME, etc.) ───────────
        // Return absolute path to bypass XDG icon theme lookup, which fails on
        // many non-Breeze icon themes despite the icon being installed in
        // hicolor and breeze fallback directories.
        NgoSenIconSearchPaths paths;
        // hicolor status/apps dirs; SVG preferred, PNG only as raster fallback.
        paths.systemDirs  = {"/usr/share/icons/hicolor/scalable/apps", "/usr/share/icons/hicolor/scalable/status", "/usr/share/icons/hicolor/22x22/status",
                             "/usr/share/icons/hicolor/24x24/status"};
        paths.fallbackDir = NGOSEN_ICON_DIR; // compile-time install dir

        iconCachePath_ = resolveNgoSenIconPath(trayIconNames(iconName, baseIconName, getEnv("XDG_CURRENT_DESKTOP")), paths);
        return iconCachePath_;
    }

    std::string NgoSenEngine::subModeLabelImpl(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        // GNOME's kimpanel shows any non-empty label in place of the icon.
        return {};
    }

    std::string NgoSenEngine::getProgramName(InputContext* ic) const {
        if (ic == nullptr) {
            return "unknown-app";
        }
        const std::string& programName = ngosen::relayedAppName(ic->program(), ic->frontend(), &shellFocusedApp_);
        return namesNoWindow(programName) ? contextRuleName(ic) : programName;
    }

    void NgoSenEngine::shellFocusedAppChanged(const std::string& app) {
        shellFocusedApp_ = app;
        if (!factory_.registered())
            return;
        // The app's focus can reach us after the field's, which then got the previous app's mode.
        instance_->inputContextManager().foreachFocused([this](InputContext* ic) {
            const std::string appName = getProgramName(ic);
            NGOSEN_INFO("App name: " + appName);
            const NgoSenMode mode = modeFor(appName);
            if (mode != realMode) {
                setMode(mode, ic);
                stateFor(ic)->reset();
                fillStatusArea(ic);
            }
            return true;
        });
    }

    void NgoSenEngine::clearAppRule(const std::string& appName) {
        {
            std::lock_guard<std::mutex> lock(appRulesMutex_);
            appRules_.erase(appName);
        }
        auto rules = *appRulesTables_.rules;
        rules.erase(std::remove_if(rules.begin(), rules.end(), [&appName](const auto& rule) { return *rule.app == appName; }), rules.end());
        appRulesTables_.rules.setValue(std::move(rules));
        if (!isStartsWith(appName, "ctx_")) {
            saveAppRules();
        }
    }
} // namespace fcitx
