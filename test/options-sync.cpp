// SPDX-License-Identifier: GPL-3.0-or-later
//
// The typing logic reads ngosen::Options, a copy of ngosen.conf. Every way the configuration changes
// (settings window, a menu toggle, the charset menu) must update that copy.
#include "ngosen-engine.h"
#include "test-input-context.h"

#include <fcitx/action.h>
#include <fcitx/menu.h>
#include <fcitx/userinterfacemanager.h>

#include <iostream>
#include <string>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    // Every option set away from its default, so a field left uncopied keeps its old value.
    fcitx::RawConfig changedConfig() {
        fcitx::RawConfig c;
        c.setValueByPath("InputMethod", "VNI");
        c.setValueByPath("SpellCheck", "False");
        c.setValueByPath("ModernStyle", "False");
        c.setValueByPath("FreeMarking", "False");
        c.setValueByPath("W2U", "Everywhere");
        c.setValueByPath("BracketTransform", "Non-Start");
        c.setValueByPath("TimeFormat", "%H:%M:%S");
        c.setValueByPath("DateFormat", "%Y-%m-%d");
        c.setValueByPath("AutoNonVnRestore", "False");
        c.setValueByPath("DdFreeStyle", "False");
        c.setValueByPath("BackspaceRemovesTone", "True");
        c.setValueByPath("EnableMacro", "False");
        c.setValueByPath("EnableMacroInOffMode", "True");
        c.setValueByPath("CapitalizeMacro", "False");
        c.setValueByPath("LockWordModifier", "Alt");
        c.setValueByPath("AutoCapitalizeAfterPunctuation", "True");
        c.setValueByPath("DoubleSpaceToPeriod", "True");
        c.setValueByPath("DoubleHyphenToEmDash", "True");
        c.setValueByPath("MessengerSelectOvertype", "False");
        c.setValueByPath("WaitSurroundingEvent", "False");
        c.setValueByPath("WaitSurroundingMinPerKeyMs", "11");
        c.setValueByPath("WaitSurroundingTimeoutMs", "222");
        c.setValueByPath("WaitSurroundingShortMs", "33");
        c.setValueByPath("WaitSurroundingSettleMs", "44");
        c.setValueByPath("WaitSurroundingSettleFirstWordMs", "55");
        c.setValueByPath("WaitSurroundingProbeEvery", "6");
        c.setValueByPath("SurrDeleteSleepMs", "7");
        c.setValueByPath("SurrCommitSleepMs", "8");
        return c;
    }

    void checkMatches(const std::string& step, const fcitx::NgoSenEngine& engine) {
        const auto& c = engine.config();
        const auto& o = engine.options();
        check(step + ": InputMethod", o.inputMethod == c.inputMethod.value());
        check(step + ": OutputCharset", o.outputCharset == c.outputCharset.value());
        check(step + ": SpellCheck", o.spellCheck == c.spellCheck.value());
        check(step + ": ModernStyle", o.modernStyle == c.modernStyle.value());
        check(step + ": FreeMarking", o.freeMarking == c.freeMarking.value());
        check(step + ": W2U", o.w2u == static_cast<int>(c.w2u.value()));
        check(step + ": BracketTransform", o.bracketTransform == static_cast<int>(c.bracketTransform.value()));
        check(step + ": TimeFormat", o.timeFormat == c.timeFormat.value());
        check(step + ": DateFormat", o.dateFormat == c.dateFormat.value());
        check(step + ": AutoNonVnRestore", o.autoNonVnRestore == c.autoNonVnRestore.value());
        check(step + ": DdFreeStyle", o.ddFreeStyle == c.ddFreeStyle.value());
        check(step + ": BackspaceRemovesTone", o.backspaceRemovesTone == c.backspaceRemovesTone.value());
        check(step + ": EnableMacro", o.enableMacro == c.enableMacro.value());
        check(step + ": EnableMacroInOffMode", o.enableMacroInOffMode == c.enableMacroInOffMode.value());
        check(step + ": CapitalizeMacro", o.capitalizeMacro == c.capitalizeMacro.value());
        check(step + ": AutoCapitalizeAfterPunctuation", o.autoCapitalizeAfterPunctuation == c.autoCapitalizeAfterPunctuation.value());
        check(step + ": CapitalizeStartOfText", o.capitalizeStartOfText == c.capitalizeStartOfText.value());
        check(step + ": DoubleSpaceToPeriod", o.doubleSpaceToPeriod == c.doubleSpaceToPeriod.value());
        check(step + ": DoubleHyphenToEmDash", o.doubleHyphenToEmDash == c.doubleHyphenToEmDash.value());
        check(step + ": MessengerSelectOvertype", o.messengerSelectOvertype == c.messengerSelectOvertype.value());
        check(step + ": WaitSurroundingEvent", o.waitSurroundingEvent == c.waitSurroundingEvent.value());
        check(step + ": WaitSurroundingMinPerKeyMs", o.waitSurroundingMinPerKeyMs == c.waitSurroundingMinPerKeyMs.value());
        check(step + ": WaitSurroundingTimeoutMs", o.waitSurroundingTimeoutMs == c.waitSurroundingTimeoutMs.value());
        check(step + ": WaitSurroundingShortMs", o.waitSurroundingShortMs == c.waitSurroundingShortMs.value());
        check(step + ": WaitSurroundingSettleMs", o.waitSurroundingSettleMs == c.waitSurroundingSettleMs.value());
        check(step + ": WaitSurroundingSettleFirstWordMs", o.waitSurroundingSettleFirstWordMs == c.waitSurroundingSettleFirstWordMs.value());
        check(step + ": WaitSurroundingProbeEvery", o.waitSurroundingProbeEvery == c.waitSurroundingProbeEvery.value());
        check(step + ": SurrDeleteSleepMs", o.surrDeleteSleepMs == c.surrDeleteSleepMs.value());
        check(step + ": SurrCommitSleepMs", o.surrCommitSleepMs == c.surrCommitSleepMs.value());
        const auto lock = c.lockWordModifier.value();
        const auto want = lock == fcitx::LockWordModifier::Shift ? ngosen::LockWordKey::Shift :
            lock == fcitx::LockWordModifier::Ctrl                ? ngosen::LockWordKey::Ctrl :
            lock == fcitx::LockWordModifier::Alt                 ? ngosen::LockWordKey::Alt :
                                                                   ngosen::LockWordKey::None;
        check(step + ": LockWordModifier", o.lockWordKey == want);
    }

    // A fresh engine must start from the core's defaults, which other frontends use as they are.
    void checkCoreDefaults(const ngosen::Options& o) {
        const ngosen::Options d;
        check("default InputMethod", o.inputMethod == d.inputMethod);
        check("default OutputCharset", o.outputCharset == d.outputCharset);
        check("default SpellCheck", o.spellCheck == d.spellCheck);
        check("default ModernStyle", o.modernStyle == d.modernStyle);
        check("default FreeMarking", o.freeMarking == d.freeMarking);
        check("default W2U", o.w2u == d.w2u);
        check("default BracketTransform", o.bracketTransform == d.bracketTransform);
        check("default TimeFormat", o.timeFormat == d.timeFormat);
        check("default DateFormat", o.dateFormat == d.dateFormat);
        check("default AutoNonVnRestore", o.autoNonVnRestore == d.autoNonVnRestore);
        check("default DdFreeStyle", o.ddFreeStyle == d.ddFreeStyle);
        check("default BackspaceRemovesTone", o.backspaceRemovesTone == d.backspaceRemovesTone);
        check("default EnableMacro", o.enableMacro == d.enableMacro);
        check("default EnableMacroInOffMode", o.enableMacroInOffMode == d.enableMacroInOffMode);
        check("default CapitalizeMacro", o.capitalizeMacro == d.capitalizeMacro);
        check("default LockWordModifier", o.lockWordKey == d.lockWordKey);
        check("default AutoCapitalizeAfterPunctuation", o.autoCapitalizeAfterPunctuation == d.autoCapitalizeAfterPunctuation);
        check("default CapitalizeStartOfText", o.capitalizeStartOfText == d.capitalizeStartOfText);
        check("default DoubleSpaceToPeriod", o.doubleSpaceToPeriod == d.doubleSpaceToPeriod);
        check("default DoubleHyphenToEmDash", o.doubleHyphenToEmDash == d.doubleHyphenToEmDash);
        check("default RetypeAfterSwitch", o.retypeAfterSwitch == d.retypeAfterSwitch);
        check("default RetoneSelection", o.retoneSelection == d.retoneSelection);
        check("default MessengerSelectOvertype", o.messengerSelectOvertype == d.messengerSelectOvertype);
        check("default WaitSurroundingEvent", o.waitSurroundingEvent == d.waitSurroundingEvent);
        check("default WaitSurroundingMinPerKeyMs", o.waitSurroundingMinPerKeyMs == d.waitSurroundingMinPerKeyMs);
        check("default WaitSurroundingTimeoutMs", o.waitSurroundingTimeoutMs == d.waitSurroundingTimeoutMs);
        check("default WaitSurroundingShortMs", o.waitSurroundingShortMs == d.waitSurroundingShortMs);
        check("default WaitSurroundingSettleMs", o.waitSurroundingSettleMs == d.waitSurroundingSettleMs);
        check("default WaitSurroundingSettleFirstWordMs", o.waitSurroundingSettleFirstWordMs == d.waitSurroundingSettleFirstWordMs);
        check("default WaitSurroundingProbeEvery", o.waitSurroundingProbeEvery == d.waitSurroundingProbeEvery);
        check("default SurrDeleteSleepMs", o.surrDeleteSleepMs == d.surrDeleteSleepMs);
        check("default SurrCommitSleepMs", o.surrCommitSleepMs == d.surrCommitSleepMs);
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-options-sync");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);
    checkMatches("defaults", engine);
    checkCoreDefaults(engine.options());

    engine.setConfig(changedConfig());
    check("the changed values took effect",
          engine.config().inputMethod.value() == "VNI" && engine.config().waitSurroundingTimeoutMs.value() == 222 &&
              engine.config().lockWordModifier.value() == fcitx::LockWordModifier::Alt);
    checkMatches("settings window", engine);

    TestInputContext context(&testInstance.instance);
    auto&            ui    = testInstance.instance.userInterfaceManager();
    auto*            macro = ui.lookupAction("ngosen-macro");
    check("macro toggle exists", macro != nullptr);
    if (macro != nullptr) {
        const bool before = engine.options().enableMacro;
        macro->activate(&context);
        check("menu toggle flips the copy", engine.options().enableMacro != before);
        checkMatches("menu toggle", engine);
    }

    auto* charsetMenu = ui.lookupAction("ngosen-charset");
    check("charset menu exists", charsetMenu != nullptr && charsetMenu->menu() != nullptr);
    if (charsetMenu != nullptr && charsetMenu->menu() != nullptr) {
        const std::string before = engine.options().outputCharset;
        for (auto* action : charsetMenu->menu()->actions()) {
            if (action->name() != "ngosen-charset-" + before) {
                action->activate(&context);
                break;
            }
        }
        check("charset menu changes the copy", engine.options().outputCharset != before);
        checkMatches("charset menu", engine);
    }

    auto* methodMenu = ui.lookupAction("ngosen-input-method");
    check("input method menu exists", methodMenu != nullptr && methodMenu->menu() != nullptr && methodMenu->menu()->actions().size() == 2);
    auto* telex = dynamic_cast<fcitx::SimpleAction*>(ui.lookupAction("ngosen-input-method-Telex"));
    auto* vni   = dynamic_cast<fcitx::SimpleAction*>(ui.lookupAction("ngosen-input-method-VNI"));
    check("input method menu lists Telex and VNI", telex != nullptr && vni != nullptr);
    if (telex != nullptr && vni != nullptr) {
        check("the current method is checked", vni->isChecked(&context) && !telex->isChecked(&context));
        telex->activate(&context);
        check("input method menu changes the copy", engine.options().inputMethod == "Telex");
        check("the picked method is checked", telex->isChecked(&context) && !vni->isChecked(&context));
        checkMatches("input method menu", engine);
    }

    return failures == 0 ? 0 : 1;
}
