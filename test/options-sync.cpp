// SPDX-License-Identifier: GPL-3.0-or-later
//
// The typing logic reads ngosen::Options, a copy of lotus.conf. Every way the configuration changes
// (settings window, a menu toggle, the charset menu) must update that copy.
#include "lotus-engine.h"
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
        c.setValueByPath("EnableMacro", "False");
        c.setValueByPath("EnableMacroInOffMode", "True");
        c.setValueByPath("CapitalizeMacro", "False");
        c.setValueByPath("MacroSkipTriggerModifier", "Ctrl");
        c.setValueByPath("AutoCapitalizeAfterPunctuation", "True");
        c.setValueByPath("DoubleSpaceToPeriod", "True");
        c.setValueByPath("DoubleHyphenToEmDash", "True");
        c.setValueByPath("useSurroundingTextIfPossible", "True");
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

    void checkMatches(const std::string& step, const fcitx::LotusEngine& engine) {
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
        check(step + ": EnableMacro", o.enableMacro == c.enableMacro.value());
        check(step + ": EnableMacroInOffMode", o.enableMacroInOffMode == c.enableMacroInOffMode.value());
        check(step + ": CapitalizeMacro", o.capitalizeMacro == c.capitalizeMacro.value());
        check(step + ": AutoCapitalizeAfterPunctuation", o.autoCapitalizeAfterPunctuation == c.autoCapitalizeAfterPunctuation.value());
        check(step + ": DoubleSpaceToPeriod", o.doubleSpaceToPeriod == c.doubleSpaceToPeriod.value());
        check(step + ": DoubleHyphenToEmDash", o.doubleHyphenToEmDash == c.doubleHyphenToEmDash.value());
        check(step + ": useSurroundingTextIfPossible", o.useSurroundingTextIfPossible == c.useSurroundingTextIfPossible.value());
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
        const auto skip = c.macroSkipTriggerModifier.value();
        const auto want = skip == fcitx::MacroSkipTriggerModifier::Shift ? ngosen::MacroSkipKey::Shift :
            skip == fcitx::MacroSkipTriggerModifier::Ctrl                ? ngosen::MacroSkipKey::Ctrl :
            skip == fcitx::MacroSkipTriggerModifier::Alt                 ? ngosen::MacroSkipKey::Alt :
                                                                           ngosen::MacroSkipKey::None;
        check(step + ": MacroSkipTriggerModifier", o.macroSkipKey == want);
    }

} // namespace

int main() {
    configureTestPaths("fcitx5-lotus-options-sync");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    checkMatches("defaults", engine);

    engine.setConfig(changedConfig());
    check("the changed values took effect",
          engine.config().inputMethod.value() == "VNI" && engine.config().waitSurroundingTimeoutMs.value() == 222 &&
              engine.config().macroSkipTriggerModifier.value() == fcitx::MacroSkipTriggerModifier::Ctrl);
    checkMatches("settings window", engine);

    TestInputContext context(&testInstance.instance);
    auto&            ui    = testInstance.instance.userInterfaceManager();
    auto*            macro = ui.lookupAction("lotus-macro");
    check("macro toggle exists", macro != nullptr);
    if (macro != nullptr) {
        const bool before = engine.options().enableMacro;
        macro->activate(&context);
        check("menu toggle flips the copy", engine.options().enableMacro != before);
        checkMatches("menu toggle", engine);
    }

    auto* charsetMenu = ui.lookupAction("lotus-charset");
    check("charset menu exists", charsetMenu != nullptr && charsetMenu->menu() != nullptr);
    if (charsetMenu != nullptr && charsetMenu->menu() != nullptr) {
        const std::string before = engine.options().outputCharset;
        for (auto* action : charsetMenu->menu()->actions()) {
            if (action->name() != "lotus-charset-" + before) {
                action->activate(&context);
                break;
            }
        }
        check("charset menu changes the copy", engine.options().outputCharset != before);
        checkMatches("charset menu", engine);
    }

    return failures == 0 ? 0 : 1;
}
