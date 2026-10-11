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
#include "ngosen-plasma-theme.h"
#include "ngosen-fcitx-host.h"
#include <optional>
#include <utility>

#include <fcitx-config/iniparser.h>
#include <fcitx/menu.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/misc.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include <fcntl.h>
#include <sstream>

namespace fcitx {
    constexpr const char* CharsetActionPrefix = "ngosen-charset-";
    const std::string     CustomKeymapFile    = "conf/ngosen-custom-keymap.conf";
    const std::string     MacroTableFile      = "conf/ngosen-macro-table.conf";

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

        void migrateLegacyMode(RawConfig& config) {
            if (const auto* mode = config.valueByPath("Mode"); mode != nullptr && isFormerSenModeName(*mode)) {
                config.setValueByPath("Mode", "Sen");
            }
            migrateLegacyModeOrder(config);
            moveOption(config, "ShowModeUinput", "ShowModeSen");
            moveOption(config, "ShortcutUinput", "ShortcutSen");
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
        // while the portal below reports the application colour scheme.  The
        // two differ in the default Fedora/Kubuntu look (#374).
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

        // GTK_THEME is honored by lightweight DEs that lack the settings
        // portal; covers XFCE, openbox, etc. with a dark theme.
        if (std::string theme = getEnv("GTK_THEME"); !theme.empty()) {
            std::transform(theme.begin(), theme.end(), theme.begin(), ::tolower);
            if (theme.find("dark") != std::string::npos) {
                cachedValue = true;
                return cachedValue;
            }
        }

        FILE* pipe = popen("dbus-send --session --dest=org.freedesktop.portal.Desktop --print-reply /org/freedesktop/portal/desktop org.freedesktop.portal.Settings.ReadOne "
                           "string:'org.freedesktop.appearance' string:'color-scheme' 2>/dev/null",
                           "r");
        if (pipe != nullptr) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                uint32_t value = 0;
                if (sscanf(buffer, "%*[^v]variant uint32 %u", &value) == 1) {
                    pclose(pipe);
                    cachedValue = value == 1;
                    return cachedValue;
                }
            }
            pclose(pipe);
        }

        pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
        if (pipe != nullptr) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                pclose(pipe);
                cachedValue = strstr(buffer, "prefer-dark") != nullptr;
                return cachedValue;
            }
            pclose(pipe);
        }

        // GTK settings file — covers DEs where the dark preference is stored
        // there instead of being exposed via portal/gsettings.
        if (std::string home = getEnv("HOME"); !home.empty()) {
            std::ifstream settingsFile(home + "/.config/gtk-3.0/settings.ini");
            if (settingsFile.is_open()) {
                std::string line;
                while (std::getline(settingsFile, line)) {
                    if (line.find("gtk-application-prefer-dark-theme=1") != std::string::npos) {
                        cachedValue = true;
                        return cachedValue;
                    }
                }
            }
            settingsFile.close();
        }

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

    ngosen::TypingState* NgoSenEngine::stateFor(InputContext* ic) {
        return &ic->propertyFor(&factory_)->state();
    }

    NgoSenEngine::NgoSenEngine(Instance* instance) :
        instance_(instance), factory_([this](InputContext& ic) {
            return new TypingStateProperty(std::make_unique<ngosen::TypingState>(this, std::make_unique<ngosen::FcitxHost>(&ic, instance_)));
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
        // A ctx_ rule names a context address, which a later window may reuse.
        contextDestroyedWatcher_ = instance_->watchEvent(EventType::InputContextDestroyed, EventWatcherPhase::Default, [this](Event& event) {
            auto appName = getProgramName(static_cast<InputContextEvent&>(event).inputContext());
            if (isStartsWith(appName, "ctx_"))
                clearAppRule(appName);
        });

        auto& uiManager = instance_->userInterfaceManager();

        charsetAction_ = std::make_unique<SimpleAction>();
        charsetAction_->setShortText(_("Charset"));
        charsetAction_->setIcon("character-set");
        uiManager.registerAction("ngosen-charset", charsetAction_.get());
        charsetMenu_ = std::make_unique<Menu>();
        charsetAction_->setMenu(charsetMenu_.get());

        auto charsets = convertToStringList(GetCharsetNames());
        for (const auto& charset : charsets) {
            charsetSubAction_.emplace_back(std::make_unique<SimpleAction>());
            auto* action = charsetSubAction_.back().get();
            action->setShortText(charset);
            action->setCheckable(true);
            uiManager.registerAction(stringutils::concat(CharsetActionPrefix, charset), action);
            connections_.emplace_back(action->connect<SimpleAction::Activated>([this, charset](InputContext* ic) {
                if (config_.outputCharset.value() == charset)
                    return;
                config_.outputCharset.setValue(charset);
                syncOptions();
                saveConfig();
                refreshEngine();
                updateCharsetAction(ic);
                if (ic)
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
            }));
            charsetMenu_->addAction(action);
        }
        config_.outputCharset.annotation().setList(charsets);

        initToggleAction(spellCheckAction_, config_.spellCheck, "ngosen-spellcheck", "tools-check-spelling", _("Spell Check"), _("Spell Check"), uiManager);
        initToggleAction(macroAction_, config_.enableMacro, "ngosen-macro", "document-edit", _("Macro"), _("Macro"), uiManager);
        initToggleAction(capitalizeMacroAction_, config_.capitalizeMacro, "ngosen-capitalizemacro", "format-text-uppercase", _("Capitalize Macro"), _("Capitalize Macro"),
                         uiManager);
        initToggleAction(autoNonVnRestoreAction_, config_.autoNonVnRestore, "ngosen-autonvnrestore", "edit-undo", _("Auto Restore Invalid Words"), _("Auto Non-VN Restore"),
                         uiManager);
        initToggleAction(enableDictionaryAction_, config_.enableDictionary, "ngosen-dictionary", "accessories-dictionary", _("Custom Dictionary"), _("Custom Dictionary"),
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

#if NGOSEN_USE_MODERN_FCITX_API
        std::string configDir = (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif

        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        reloadConfig();
        realMode = config_.mode.value();
        instance_->inputContextManager().registerProperty("NgoSenState", &factory_);
        appRulesPath_ = configDir + "/ngosen-app-rules.conf";
        loadAppRules();
        toggleActions_ = {charsetAction_.get(),          spellCheckAction_.get(),       macroAction_.get(),   capitalizeMacroAction_.get(),
                          autoNonVnRestoreAction_.get(), enableDictionaryAction_.get(), saveLogAction_.get(), settingsAction_.get()};
    }

    void NgoSenEngine::initToggleAction(std::unique_ptr<SimpleAction>& action, Option<bool>& option, const std::string& actionId, const std::string& iconName,
                                        const std::string& textLong, const std::string& textOnOff, UserInterfaceManager& uiManager) {
        action = std::make_unique<SimpleAction>();
        action->setShortText(textLong);
        action->setIcon(iconName);
        action->setCheckable(false);
        connections_.emplace_back(action->connect<SimpleAction::Activated>([this, &action, &option, textOnOff](InputContext* ic) {
            option.setValue(!option.value());
            syncOptions();
            saveConfig();
            refreshOption();
            updateAction(ic, action, option, textOnOff);
        }));
        uiManager.registerAction(actionId, action.get());
    }

    void NgoSenEngine::updateAction(InputContext* ic, std::unique_ptr<SimpleAction>& action, Option<bool>& option, const std::string& textOnOff) {
        action->setShortText((option.value() ? "✔ " : "✖ ") + textOnOff);
        if (ic != nullptr) {
            action->update(ic);
        }
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
        config_.load(raw);
        readAsIni(customKeymap_, CustomKeymapFile);
        readAsIni(macroTables_, MacroTableFile);
        macroTableObject_.reset(newMacroTable(macroTables_));
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
                if (!isStartsWith(p.string(), "/home/")) {
                    auto fd = fcitx::UnixFD(::open(p.c_str(), O_RDONLY));
                    if (fd.isValid()) {
                        dictionary_.reset(NewDictionary(fd.release()));
#else
                if (!isStartsWith(p, "home/")) {
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

    const Configuration* NgoSenEngine::getSubConfig(const std::string& path) const {
        if (path == "custom_keymap")
            return &customKeymap_;
        if (path == "ngosen-macro") {
            return &macroTables_;
        }
        if (path == "app_rules") {
            return &appRulesTables_;
        }
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
        o.enableMacro                      = config_.enableMacro.value();
        o.enableMacroInOffMode             = config_.enableMacroInOffMode.value();
        o.capitalizeMacro                  = config_.capitalizeMacro.value();
        o.autoCapitalizeAfterPunctuation   = config_.autoCapitalizeAfterPunctuation.value();
        o.doubleSpaceToPeriod              = config_.doubleSpaceToPeriod.value();
        o.doubleHyphenToEmDash             = config_.doubleHyphenToEmDash.value();
        o.useSurroundingTextIfPossible     = config_.useSurroundingTextIfPossible.value();
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
        switch (config_.macroSkipTriggerModifier.value()) {
            case MacroSkipTriggerModifier::Shift: o.macroSkipKey = ngosen::MacroSkipKey::Shift; break;
            case MacroSkipTriggerModifier::Ctrl: o.macroSkipKey = ngosen::MacroSkipKey::Ctrl; break;
            case MacroSkipTriggerModifier::Alt: o.macroSkipKey = ngosen::MacroSkipKey::Alt; break;
            case MacroSkipTriggerModifier::Disabled:
            default: o.macroSkipKey = ngosen::MacroSkipKey::None; break;
        }
    }

    void NgoSenEngine::populateConfig() {
        syncOptions();
        refreshEngine();
        refreshOption();
        updateCharsetAction(nullptr);
        updateAction(nullptr, spellCheckAction_, config_.spellCheck, _("Spell Check"));
        updateAction(nullptr, macroAction_, config_.enableMacro, _("Macro"));
        updateAction(nullptr, capitalizeMacroAction_, config_.capitalizeMacro, _("Capitalize Macro"));
        updateAction(nullptr, autoNonVnRestoreAction_, config_.autoNonVnRestore, _("Auto Non-VN Restore"));
        updateAction(nullptr, enableDictionaryAction_, config_.enableDictionary, _("Custom Dictionary"));
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
        }
    }

    std::string NgoSenEngine::subMode(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        return *config_.inputMethod;
    }

    void NgoSenEngine::activate(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        auto* ic = event.inputContext();
        if (dropStaleSurroundingText(ic))
            NGOSEN_INFO("Dropped stale surrounding text");
        static std::atomic<bool> mouseThreadStarted{false};
        if (!mouseThreadStarted.exchange(true))
            startMouseReset();

        auto& statusArea = event.inputContext()->statusArea();
        if (ic->capabilityFlags().test(CapabilityFlag::Preedit))
            instance_->inputContextManager().setPreeditEnabledByDefault(true);

        std::string appName = getProgramName(ic);
        NGOSEN_INFO("App name: " + appName);

        const NgoSenMode targetMode = getAppRule(appName);
        NGOSEN_INFO("Target mode: " + ngosen::ModeI18NAnnotation::toString(targetMode));

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
        for (const auto& action : toggleActions_) {
            statusArea.addAction(StatusGroup::InputMethod, action);
        }
    }

    void NgoSenEngine::keyEvent(const InputMethodEntry& /*entry*/, KeyEvent& keyEvent) {
        auto* ic = keyEvent.inputContext();
        dropStaleSurroundingText(ic);

        if (isSelectingAppMode_ && g_mouse_clicked.load(std::memory_order_acquire)) {
            closeAppModeMenu();
            closeModeMenuPanel(ic, true);
        }

        if (isSelectingAppMode_) {
            handleModeMenuKey(keyEvent);
            return;
        }

        if (!keyEvent.isRelease() && !config_.cycleModeKey->empty() && keyEvent.key().checkKeyList(*config_.cycleModeKey)) {
            cycleMode(ic);
            keyEvent.filterAndAccept();
            return;
        }

        if (!keyEvent.isRelease() && !config_.modeMenuKey->empty() && keyEvent.key().checkKeyList(*config_.modeMenuKey)) {
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
        }

        closeModeMenuPanel(ic, true);
        setMode(mode, ic);
        if (mode == NgoSenMode::Emoji) {
            stateFor(ic)->updateEmojiPreedit();
        } else {
            showCycleModeNotification(mode, ic);
        }
    }

    std::vector<NgoSenMode> NgoSenEngine::cycleModes() {
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
                mode = config().mode.value();
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
        const NgoSenMode  current      = getAppRule(appName);
        const auto        enabledModes = cycleModes();
        if (enabledModes.empty())
            return;

        auto       it       = std::find(enabledModes.begin(), enabledModes.end(), current);
        NgoSenMode nextMode = it == enabledModes.end() ? enabledModes[0] : enabledModes[(static_cast<size_t>(it - enabledModes.begin()) + 1) % enabledModes.size()];
        setMode(nextMode, ic);
        setAppRule(appName, nextMode);
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
        setMode(getAppRule(currentConfigureApp_), ic);
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
                setMode(getAppRule(getProgramName(ic)), ic);
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

    void NgoSenEngine::updateCharsetAction(InputContext* ic) {
        auto name = stringutils::concat(CharsetActionPrefix, *config_.outputCharset);
        for (const auto& action : charsetSubAction_) {
            action->setChecked(action->name() == name);
            if (ic != nullptr)
                action->update(ic);
        }
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

    NgoSenMode NgoSenEngine::getAppRule(const std::string& appName) const {
        std::lock_guard<std::mutex> lock(appRulesMutex_);
        auto                        it = appRules_.find(appName);
        if (it == appRules_.end()) {
            // Exact match wins so a "foo.desktop" rule can still override "foo".
            it = appRules_.find(stripDesktopSuffix(appName));
        }
        if (it != appRules_.end()) {
            return it->second;
        }
        return config_.mode.value();
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
            {"Default", {config_.mode.value(), _("Default Typing"), getShortcut(*config_.shortcutDefault), *config_.showModeDefault}}};

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
        const NgoSenMode defaultMode = config_.mode.value();

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
            } else if (isDefaultItem && getAppRule(currentConfigureApp_) == defaultMode && appRules_.find(currentConfigureApp_) == appRules_.end()) {
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

    void NgoSenEngine::saveTypingLog(InputContext* ic) {
        const std::string     stateHome = getEnv("XDG_STATE_HOME");
        std::filesystem::path dir       = stateHome.empty() ? std::filesystem::path(getEnv("HOME")) / ".local" / "state" : std::filesystem::path(stateHome);
        dir /= "ngosen";
        std::error_code error;
        std::filesystem::create_directories(dir, error);

        char       stamp[32];
        const auto now = std::time(nullptr);
        std::tm    local{};
        localtime_r(&now, &local);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
        const auto    path = dir / (std::string("typing-") + stamp + ".log");
        std::ofstream out(path);
        out << "# Ngó Sen " << NGOSEN_VERSION << " typing log. It holds what you typed just before saving; read it before sharing.\n"
            << "# desktop=" << getEnv("XDG_CURRENT_DESKTOP") << " session=" << getEnv("XDG_SESSION_TYPE") << "\n"
            << recorder_.dump();
        out.close();

        const std::string message = out ? _("Typing log saved: ") + path.string() : _("Could not save the typing log to ") + dir.string();
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

        // ── Cinnamon: return icon NAME (not absolute path) ──────────────────
        // Cinnamon's tray uses XApp Status Applet (SNI).  The IconName property
        // is sent over D-Bus and resolved via Gtk.IconTheme — which only
        // understands theme icon names, not filesystem paths.
        //
        // On KDE and GNOME, absolute paths work correctly — their compositors
        // or SNI hosts handle filesystem paths in IconName.
        static const bool kIsCinnamon = [] {
            std::string de = getEnv("XDG_CURRENT_DESKTOP");
            if (de.empty())
                de = getEnv("DESKTOP_SESSION");
            return !de.empty() && (de == "cinnamon" || de == "X-Cinnamon");
        }();

        if (kIsCinnamon) {
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

        iconCachePath_ = resolveNgoSenIconPath({iconName, baseIconName}, paths);
        return iconCachePath_;
    }

    std::string NgoSenEngine::subModeLabelImpl(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        switch (realMode) {
            case NgoSenMode::Off: return _("Ngó Sen - Off");
            case NgoSenMode::Emoji: return "😄";
            default: return "vi";
        }
    }

    std::string NgoSenEngine::getProgramName(InputContext* ic) {
        if (ic == nullptr) {
            return "unknown-app";
        }
        std::string programName = ic->program();
        if (programName.empty() || programName == "wayland" || programName == "x11") {
            // Fallback: InputContext address-based resolution
            // This ensures at least per-window separation.
            std::ostringstream oss;
            oss << "ctx_" << static_cast<const void*>(ic);
            programName = oss.str();
        }
        return programName;
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
