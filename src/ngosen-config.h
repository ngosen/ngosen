/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file ngosen-config.h
 * @brief Configuration definitions for the Ngó Sen input method.
 */

#ifndef _FCITX5_NGOSEN_CONFIG_H_
#define _FCITX5_NGOSEN_CONFIG_H_

#include <cstdint>
#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/stringutils.h>

#include "ngosen-mode.h"
#include "ngosen-options.h"

namespace ngosen {

    // Config reads and writes find these helpers by argument-dependent lookup, so they sit next to Mode.
    FCITX_CONFIG_ENUM_NAME_WITH_I18N(Mode, N_("OFF"), N_("Sen"), N_("Preedit"), N_("Emoji Picker"));

} // namespace ngosen

namespace fcitx {

    using NgoSenMode = ngosen::Mode;

    /**
     * @brief Converts NgoSenMode to int and vice versa.
     */
    int        modeToInt(NgoSenMode mode);
    NgoSenMode intToMode(int mode);

    /**
     * @brief W2U mode for w to ư conversion.
     */
    enum class W2UMode : std::uint8_t {
        Disabled   = 0,
        NonStart   = 1,
        Everywhere = 2,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(W2UMode, N_("Disabled"), N_("Non-Start"), N_("Everywhere"));

    /**
     * @brief Bracket transform mode for [ -> ơ, ] -> ư conversion.
     */
    enum class BracketTransformMode : std::uint8_t {
        Disabled   = 0,
        NonStart   = 1,
        Everywhere = 2,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(BracketTransformMode, N_("Disabled"), N_("Non-Start"), N_("Everywhere"));

    /**
     * @brief Modifier whose tap keeps the word being typed as it is.
     */
    enum class LockWordModifier : std::uint8_t {
        Disabled = 0,
        Shift    = 1,
        Ctrl     = 2,
        Alt      = 3,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(LockWordModifier, N_("Disabled"), N_("Shift"), N_("Ctrl"), N_("Alt"));

    /**
     * @brief Icon theme options.
     */
    enum class IconTheme : std::uint8_t {
        Auto,
        Light,
        Dark,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(IconTheme, N_("Auto"), N_("Light"), N_("Dark"));

    enum class UiLanguage : std::uint8_t {
        Vietnamese,
        English,
    };

    FCITX_CONFIG_ENUM_NAME(UiLanguage, "Vietnamese", "English");

    struct InputMethodConstrain;
    struct InputMethodAnnotation;

    using InputMethodOption = Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation>;

    /**
     * @brief Annotation for string list options in configuration UI.
     */
    struct StringListAnnotation : public EnumAnnotation {
        /**
         * @brief Sets the string list.
         * @param list Vector of strings to set.
         */
        void setList(std::vector<std::string> list) {
            list_ = std::move(list);
        }

        /**
         * @brief Gets the string list.
         * @return Reference to the list.
         */
        const auto& list() {
            return list_;
        }

        /**
         * @brief Dumps description to config.
         * @param config Config to write to.
         */
        void dumpDescription(RawConfig& config) const {
            EnumAnnotation::dumpDescription(config);
            config.setValueByPath("IsEnum", "True");
            for (size_t i = 0; i < list_.size(); ++i) {
                config.setValueByPath("Enum/" + std::to_string(i), list_[i]);
            }
        }

      protected:
        std::vector<std::string> list_; // NOLINT
    };

    struct InputMethodAnnotation : public StringListAnnotation {
        /**
         * @brief Dumps description with sub-config paths.
         * @param config Config to write to.
         */
        void dumpDescription(RawConfig& config) const {
            StringListAnnotation::dumpDescription(config);
            config.setValueByPath("LaunchSubConfig", "True");
        }
    };

    /**
     * @brief Annotation for time format list.
     */
    struct TimeFormatAnnotation : public StringListAnnotation {
        TimeFormatAnnotation() {
            list_ = {"%H:%M", "%H:%M:%S", "%I:%M:%S %p", "%I:%M %p", ""};
        }
    };

    /**
     * @brief Annotation for date format list.
     */
    struct DateFormatAnnotation : public StringListAnnotation {
        DateFormatAnnotation() {
            list_ = {"%d/%m/%Y", "%m/%d/%Y", "%Y-%m-%d", "%d/%m/%y", "%y-%m-%d", ""};
        }
    };

    /**
     * @brief Constraint validator for input method options.
     */
    struct InputMethodConstrain {
        using Type = std::string;

        /**
         * @brief Constructs with option pointer.
         * @param option Pointer to input method option.
         */
        InputMethodConstrain(const InputMethodOption* option) : option_(option) {}

        /**
         * @brief Validates if name is in the allowed list.
         * @param name Name to check.
         * @return True if valid.
         */
        bool check(const std::string& name) const {
            const auto& list = option_->annotation().list();
            if (list.empty()) {
                return true;
            }
            return std::find(list.begin(), list.end(), name) != list.end();
        }

        /**
         * @brief Dumps description (no-op).
         * @param config Unused.
         */
        void dumpDescription(RawConfig& /*unused*/) const {}

      private:
        const InputMethodOption* option_;
    };

    FCITX_CONFIGURATION(ngosenKeymap, Option<std::string> key{this, "Key", _("Key"), ""}; Option<std::string> value{this, "Value", _("Value"), ""};);

    FCITX_CONFIGURATION(ngosenMacroTable,
                        OptionWithAnnotation<std::vector<ngosenKeymap>, ListDisplayOptionAnnotation> macros{
                            this, "Macro", _("Macro"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(ngosenCustomKeymap,
                        OptionWithAnnotation<std::vector<ngosenKeymap>, ListDisplayOptionAnnotation> customKeymap{
                            this, "CustomKeymap", _("Custom Keymap"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(ngosenAppRule, Option<std::string> app{this, "App", _("App"), ""}; Option<int> mode{this, "Mode", _("Mode"), 0};);
    FCITX_CONFIGURATION(ngosenAppRules,
                        OptionWithAnnotation<std::vector<ngosenAppRule>, ListDisplayOptionAnnotation> rules{
                            this, "Rules", _("Rules"), {}, {}, {}, ListDisplayOptionAnnotation("App")};);

    /**
     * @brief Main configuration structure for the Ngó Sen input method.
     */
    FCITX_CONFIGURATION(
        ngosenConfig,

        OptionWithAnnotation<NgoSenMode, ngosen::ModeI18NAnnotation>                                     mode{this, "Mode", _("Mode"), NgoSenMode::Sen};
        Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation> inputMethod{
            this, "InputMethod", _("Input Method"), ngosen::defaultOptions.inputMethod, InputMethodConstrain(&inputMethod), {}, InputMethodAnnotation()};
        OptionWithAnnotation<std::string, StringListAnnotation> outputCharset{
            this, "OutputCharset", _("Output Charset"), ngosen::defaultOptions.outputCharset, {}, {}, StringListAnnotation()};
        // Ctrl+Shift as in UniKey, pressed in either order; ngosen-engine.cpp keeps Ctrl+Space for older installs.
        KeyListOption toggleKey{this,
                                "ToggleKey",
                                _("Switch Between English and Vietnamese"),
                                {Key("Control+Shift_L"), Key("Control+Shift_R"), Key("Shift+Control_L"), Key("Shift+Control_R")},
                                KeyListConstrain({KeyConstrainFlag::AllowModifierLess, KeyConstrainFlag::AllowModifierOnly})};
        // What the toggle key last chose, for apps without a rule of their own.
        HiddenOption<bool> vietnameseOn{this, "VietnameseOn", _("Vietnamese On"), false};
        // Set once the settings were upgraded; fcitx5 leaves defaults out of the file, so nothing else tells.
        HiddenOption<int> settingsVersion{this, "SettingsVersion", _("Settings Version"), 0}; KeyListOption modeMenuKey{
            this, "ModeMenuKey", _("Mode Menu Hotkey"), {Key("grave")}, KeyListConstrain({KeyConstrainFlag::AllowModifierLess, KeyConstrainFlag::AllowModifierOnly})};
        KeyListOption cycleModeKey{this, "CycleModeKey", _("Cycle Mode Hotkey"), {}, KeyListConstrain({KeyConstrainFlag::AllowModifierLess, KeyConstrainFlag::AllowModifierOnly})};
        SubConfigOption                                                                appRules{this, "AppRules", _("App Rules"), "fcitx://config/addon/ngosen/app_rules"};
        OptionWithAnnotation<W2UMode, W2UModeI18NAnnotation>                           w2u{this, "W2U", _("Type w to Produce ư"), static_cast<W2UMode>(ngosen::defaultOptions.w2u)};
        OptionWithAnnotation<BracketTransformMode, BracketTransformModeI18NAnnotation> bracketTransform{this, "BracketTransform", _("Type [ -> ơ, ] -> ư, { -> Ơ, } -> Ư"),
                                                                                                        static_cast<BracketTransformMode>(ngosen::defaultOptions.bracketTransform)};

        Option<bool> spellCheck{this, "SpellCheck", _("Enable Spell Check Using Dictionary"), ngosen::defaultOptions.spellCheck};
        Option<bool> enableMacro{this, "EnableMacro", _("Enable Macro"), ngosen::defaultOptions.enableMacro};
        Option<bool> capitalizeMacro{this, "CapitalizeMacro", _("Capitalize Macro"), ngosen::defaultOptions.capitalizeMacro};
        OptionWithAnnotation<LockWordModifier, LockWordModifierI18NAnnotation> lockWordModifier{this, "LockWordModifier", _("Key to Keep the Word as Typed"),
                                                                                                static_cast<LockWordModifier>(ngosen::defaultOptions.lockWordKey)};
        Option<bool> autoCapitalizeAfterPunctuation{this, "AutoCapitalizeAfterPunctuation", _("Auto capitalize after sentence-ending punctuation (. ! ? Enter)"),
                                                    ngosen::defaultOptions.autoCapitalizeAfterPunctuation};
        Option<bool> capitalizeStartOfText{this, "CapitalizeStartOfText", _("Capitalize the first letter of the text or of a line"), ngosen::defaultOptions.capitalizeStartOfText};
        Option<bool> doubleSpaceToPeriod{this, "DoubleSpaceToPeriod", _("Double Space to Period"), ngosen::defaultOptions.doubleSpaceToPeriod};
        Option<bool> doubleHyphenToEmDash{this, "DoubleHyphenToEmDash", _("Double Hyphen to Em-Dash (--)"), ngosen::defaultOptions.doubleHyphenToEmDash};
        Option<bool> retypeAfterSwitch{this, "RetypeAfterSwitch", _("Retype text typed before switching on"), ngosen::defaultOptions.retypeAfterSwitch};
        Option<bool> retoneSelection{this, "RetoneSelection", _("Change the tone of a selected word"), ngosen::defaultOptions.retoneSelection};
        Option<bool> autoNonVnRestore{this, "AutoNonVnRestore", _("Auto Restore Invalid Words"), ngosen::defaultOptions.autoNonVnRestore};
        Option<bool> modernStyle{this, "ModernStyle", _("Use oà, uý (Instead Of òa, úy)"), ngosen::defaultOptions.modernStyle};
        Option<bool> freeMarking{this, "FreeMarking", _("Allow Type With More Freedom"), ngosen::defaultOptions.freeMarking};
        Option<bool> ddFreeStyle{this, "DdFreeStyle", _("Allow dd To Produce đ When Auto Restore Invalid Words Is On"), ngosen::defaultOptions.ddFreeStyle};
        Option<bool> backspaceRemovesTone{this, "BackspaceRemovesTone", _("Backspace Removes the Tone Mark"), ngosen::defaultOptions.backspaceRemovesTone};
        // Timing for deleting through the app. Hidden from the config window; tests and bug reports
        // still set them in ngosen.conf.
        HiddenOption<bool> waitSurroundingEvent{this, "WaitSurroundingEvent", _("Experiment: after forwarded backspaces, wait for the surrounding-text event instead of sleeping"),
                                                ngosen::defaultOptions.waitSurroundingEvent};
        HiddenOption<int>  waitSurroundingTimeoutMs{this, "WaitSurroundingTimeoutMs", _("Experiment: timeout (ms) for that wait"), ngosen::defaultOptions.waitSurroundingTimeoutMs};
        HiddenOption<int> waitSurroundingShortMs{this, "WaitSurroundingShortMs", _("Experiment: shorter timeout (ms) used after two consecutive timeouts without a matching event"),
                                                 ngosen::defaultOptions.waitSurroundingShortMs};
        HiddenOption<int> waitSurroundingProbeEvery{this, "WaitSurroundingProbeEvery",
                                                    _("Experiment: when the app's snapshot is frozen during deletion, stop waiting and re-probe once every N replacements"),
                                                    ngosen::defaultOptions.waitSurroundingProbeEvery};
        HiddenOption<int> waitSurroundingSettleMs{this, "WaitSurroundingSettleMs", _("Experiment: extra ms to wait after the deletion shows done, Messenger composer only"),
                                                  ngosen::defaultOptions.waitSurroundingSettleMs};
        HiddenOption<bool> messengerSelectOvertype{this, "MessengerSelectOvertype", _("Experiment: select with Shift+Left and type over it in the Messenger composer"),
                                                   ngosen::defaultOptions.messengerSelectOvertype};
        HiddenOption<int>  waitSurroundingSettleFirstWordMs{this, "WaitSurroundingSettleFirstWordMs",
                                                            _("Experiment: settle wait for the first word of a Messenger message (composer still loading)"),
                                                            ngosen::defaultOptions.waitSurroundingSettleFirstWordMs};
        HiddenOption<int>  waitSurroundingMinPerKeyMs{this, "WaitSurroundingMinPerKeyMs",
                                                      _("Experiment: a snapshot identical to the one at send time counts as done only after this many ms per backspace"),
                                                      ngosen::defaultOptions.waitSurroundingMinPerKeyMs};
        HiddenOption<int>  surrDeleteSleepMs{this, "SurrDeleteSleepMs", _("Experiment: ms per deleted char to sleep after deleteSurroundingText"),
                                             ngosen::defaultOptions.surrDeleteSleepMs};
        HiddenOption<int>  surrCommitSleepMs{this, "SurrCommitSleepMs", _("Experiment: ms per committed char to sleep after commitString on the surrounding-text path"),
                                             ngosen::defaultOptions.surrCommitSleepMs};
        // The key keeps its old name so saved settings still load.
        Option<bool> useNgoSenIcons{this, "UseLotusIcons", _("Use Ngó Sen Status Icons"), true};

        // Settings and Save typing log have no option: without them the menu could not lead back.
        Option<bool> showMenuInputMethod{this, "ShowMenuInputMethod", _("Show Input Method in the tray menu"), true};
        Option<bool> showMenuCharset{this, "ShowMenuCharset", _("Show Charset in the tray menu"), true};
        Option<bool> showMenuSpellCheck{this, "ShowMenuSpellCheck", _("Show Spell Check in the tray menu"), true};
        Option<bool> showMenuMacro{this, "ShowMenuMacro", _("Show Macro in the tray menu"), true};
        Option<bool> showMenuCapitalizeMacro{this, "ShowMenuCapitalizeMacro", _("Show Capitalize Macro in the tray menu"), true};
        Option<bool> showMenuAutoCapitalize{this, "ShowMenuAutoCapitalize", _("Show Auto Capitalize in the tray menu"), true};
        Option<bool> showMenuAutoRestore{this, "ShowMenuAutoRestore", _("Show Auto Restore Invalid Words in the tray menu"), true};
        Option<bool> showMenuDictionary{this, "ShowMenuDictionary", _("Show Custom Dictionary in the tray menu"), true};

        Option<bool> enableDictionary{this, "EnableDictionary", _("Custom Dictionary"), false};
        Option<bool> enableCustomKeymap{this, "EnableCustomKeymap", _("Custom Keymap"), false};

        Option<bool> showModeSen{this, "ShowModeSen", _("Show Sen"), true}; Option<std::string> shortcutSen{this, "ShortcutSen", _("Shortcut for Sen"), "1"};
        Option<bool>                                                                            showModePreedit{this, "ShowModePreedit", _("Show Preedit"), true};
        Option<std::string>                                                                     shortcutPreedit{this, "ShortcutPreedit", _("Shortcut for Preedit"), "q"};
        Option<bool>                                                                            showModeEmoji{this, "ShowModeEmoji", _("Show Emoji Picker"), true};
        Option<std::string> shortcutEmoji{this, "ShortcutEmoji", _("Shortcut for Emoji Picker"), "w"}; Option<bool> showModeOff{this, "ShowModeOff", _("Show OFF"), true};
        Option<std::string> shortcutOff{this, "ShortcutOff", _("Shortcut for OFF"), "e"}; Option<bool> showModeDefault{this, "ShowModeDefault", _("Show Default Typing"), true};
        Option<std::string> shortcutDefault{this, "ShortcutDefault", _("Shortcut for Default Typing"), "r"};
        Option<bool>        enableMacroInOffMode{this, "EnableMacroInOffMode", _("Allow Macro in Off Mode"), ngosen::defaultOptions.enableMacroInOffMode};

        Option<std::string> modeOrder{this, "ModeOrder", _("Mode Order"), "Sen,Preedit,Emoji,Off,Default"};

        OptionWithAnnotation<std::string, TimeFormatAnnotation> timeFormat{
            this, "TimeFormat", _("Time Format ($TIME in macro)"), ngosen::defaultOptions.timeFormat, {}, {}, TimeFormatAnnotation()};
        OptionWithAnnotation<std::string, DateFormatAnnotation> dateFormat{
            this, "DateFormat", _("Date Format ($DATE in macro)"), ngosen::defaultOptions.dateFormat, {}, {}, DateFormatAnnotation()};

        SubConfigOption                                          macroEditor{this, "MacroEditor", _("Macro"), "fcitx://config/addon/ngosen/ngosen-macro"};
        SubConfigOption                                          customKeymap{this, "CustomKeymap", _("Custom Keymap"), "fcitx://config/addon/ngosen/custom_keymap"};
        OptionWithAnnotation<IconTheme, IconThemeI18NAnnotation> iconTheme{this, "IconTheme", _("Icon Color"), IconTheme::Auto};
        Option<UiLanguage>                                       uiLanguage{this, "UiLanguage", _("Interface Language"), UiLanguage::Vietnamese};);

} // namespace fcitx

#endif
