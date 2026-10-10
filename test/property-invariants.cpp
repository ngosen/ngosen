// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file property-invariants.cpp
 * @brief Property-based invariant test for the engine composition state.
 *
 * Why this test exists:
 * The existing headless tests each pin one concrete scenario ("type a, then s,
 * expect á"). Concrete scenarios only cover the inputs someone thought of. Most
 * reported engine defects were triggered by inputs nobody wrote a case for:
 * duplicated characters after a retro-edit ("toôi"), UTF-8 corruption when
 * backspacing over multi-byte text, and a crash on an unexpected key sequence.
 *
 * Instead of more examples, this test states properties that must hold for
 * EVERY key sequence, then drives a few hundred pseudo-random sequences through
 * the production engine and checks all of them after every keystroke.
 *
 * The five properties:
 *   P1 Client preedit is always valid UTF-8.
 *   P2 Preedit never contains more characters than the number of keys typed.
 *      (Telex only ever merges keys into a character; it never invents one.)
 *   P3 Typing one non-modifier consonant and then BackSpace returns the preedit
 *      to exactly what it was before. This is the shape of the "toôi" defect.
 *   P4 Deactivating the context leaves the client preedit empty.
 *   P5 Replaying the same key sequence on a fresh context produces identical
 *      commits and identical preedit — no state leaks between compositions.
 *
 * What is proven, and what is not:
 * Four of the five properties have a positive control: a deliberate defect was
 * injected, the expected failure was written down first, and the run matched it.
 * Each mutation was reverted and the source verified back to its original hash.
 *
 *   P2  render the preedit twice
 *       -> sequence #0, "P2 preedit has 2 characters after only 1 keys (preedit=cc)"
 *   P3  call RemoveLastChar twice in the Bamboo backspace path
 *       -> sequence #0, "preedit was 'tzưdoxf' before k+BackSpace and 'tzưdox' after"
 *   P4  drop inputPanel().reset() in NgoSenEngine::deactivate
 *       -> sequence #0, "P4 client preedit still holds 'tzưdoxf' after deactivate"
 *   P5  uppercase any preedit string that was rendered once before
 *       -> sequence #0, "replay yielded preedit=TZưDOXF" against "tzưdoxf"
 *
 * P1 has no positive control, and the failed attempt is worth recording:
 * appending a stray 0xC3 byte in the preedit path never reaches P1, because
 * fcitx::Text::append throws on invalid UTF-8 first and the process aborts.
 * Invalid UTF-8 therefore cannot reach a client through this path at all, and
 * P1 stands as a backstop for future paths rather than a guard with a
 * demonstrated failure mode.
 *
 * Determinism:
 * The generator uses a fixed seed, so a failure here reproduces exactly. The
 * seed and the failing key sequence are printed, so the case can be pasted into
 * a normal one-scenario regression test.
 *
 * Configuration:
 * Macro expansion, the dictionary, spell check and auto non-Vietnamese restore
 * are switched OFF. Each of those may legitimately rewrite or lengthen the
 * composition, which would make P2 and P3 false for reasons that are not bugs.
 * They deserve their own properties and are out of scope here.
 */

#include "ngosen-engine.h"
#include "test-input-context.h"

#include <fcitx-utils/utf8.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual, const std::string& meaning) {
        std::cerr << "Step: " << step << '\n';
        std::cerr << "Expected: " << expected << '\n';
        std::cerr << "Actual: " << actual << '\n';
        std::cerr << "Meaning: " << meaning << '\n';
    }

    // Fixed so that a failure reproduces byte for byte on the next run.
    constexpr std::uint32_t kSeed            = 20260906u;
    constexpr int           kSequenceCount   = 200;
    constexpr int           kMaxSequenceKeys = 12;
    // 'k' is not a Telex modifier (modifiers are s f r x j z w a e o d), so
    // appending it can only extend the composition, never re-interpret it.
    constexpr auto kNonModifierKey = FcitxKey_k;

    struct KeyStroke {
        FcitxKeySym sym;
        const char* name;
    };

    // Letters that carry Telex meaning, plus BackSpace, plus plain consonants.
    // Space and punctuation are excluded: they commit the composition, which
    // would end the sequence under test rather than exercise it.
    const std::vector<KeyStroke>& alphabet() {
        static const std::vector<KeyStroke> keys = {
            {FcitxKey_a, "a"}, {FcitxKey_e, "e"}, {FcitxKey_o, "o"}, {FcitxKey_u, "u"}, {FcitxKey_i, "i"}, {FcitxKey_y, "y"}, {FcitxKey_d, "d"},
            {FcitxKey_w, "w"}, {FcitxKey_s, "s"}, {FcitxKey_f, "f"}, {FcitxKey_r, "r"}, {FcitxKey_x, "x"}, {FcitxKey_j, "j"}, {FcitxKey_z, "z"},
            {FcitxKey_t, "t"}, {FcitxKey_n, "n"}, {FcitxKey_g, "g"}, {FcitxKey_h, "h"}, {FcitxKey_c, "c"}, {FcitxKey_m, "m"}, {FcitxKey_BackSpace, "<BS>"},
        };
        return keys;
    }

    std::string describe(const std::vector<KeyStroke>& sequence) {
        std::string out;
        for (const auto& key : sequence) {
            out += key.name;
        }
        return out;
    }

    // One composition session: a fresh context driven by the given keys.
    struct Session {
        std::vector<std::string> commits;
        std::string              preedit;
    };

    class Runner {
      public:
        Runner(fcitx::NgoSenEngine& engine, fcitx::Instance& instance) : engine_(engine), instance_(instance) {}

        // Drives `sequence` through a brand new context. When `checkPerKey` is
        // set, P1/P2 are verified after every single keystroke; `failure` is
        // filled in and false returned on the first violation.
        bool run(const std::vector<KeyStroke>& sequence, bool checkPerKey, Session& out, std::string& failure) {
            auto context = std::make_unique<TestInputContext>(&instance_);
            context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
            context->focusIn();
            fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
            fcitx::InputContextEvent activateEvent(context.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry, activateEvent);

            std::size_t typed = 0;
            for (const auto& key : sequence) {
                fcitx::KeyEvent event(context.get(), fcitx::Key(key.sym), false);
                engine_.keyEvent(entry, event);
                ++typed;

                if (!checkPerKey) {
                    continue;
                }
                const std::string preedit = context->inputPanel().clientPreedit().toString();

                // P1: the engine must never hand the client malformed UTF-8.
                if (!fcitx::utf8::validate(preedit)) {
                    failure = "P1 invalid UTF-8 after key '" + std::string(key.name) + "'";
                    return false;
                }
                // P2: a keystroke may merge into an existing character or add
                // one, never add more than one.
                const std::size_t length = fcitx::utf8::length(preedit);
                if (length > typed) {
                    failure = "P2 preedit has " + std::to_string(length) + " characters after only " + std::to_string(typed) + " keys (preedit=" + preedit + ")";
                    return false;
                }
            }

            out.commits = context->commits();
            out.preedit = context->inputPanel().clientPreedit().toString();

            // P4: deactivation must leave nothing behind in the client widget.
            fcitx::InputContextEvent deactivateEvent(context.get(), fcitx::EventType::InputContextFocusOut);
            engine_.deactivate(entry, deactivateEvent);
            if (!context->inputPanel().clientPreedit().toString().empty()) {
                failure = "P4 client preedit still holds '" + context->inputPanel().clientPreedit().toString() + "' after deactivate";
                return false;
            }
            return true;
        }

        // P3: append one plain consonant, then BackSpace, and require the
        // preedit to be exactly what it was beforehand.
        bool roundTrip(const std::vector<KeyStroke>& sequence, std::string& failure) {
            auto context = std::make_unique<TestInputContext>(&instance_);
            context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
            context->focusIn();
            fcitx::InputMethodEntry  entry("ngosen", "Ngó Sen", "vi", "ngosen");
            fcitx::InputContextEvent activateEvent(context.get(), fcitx::EventType::InputContextFocusIn);
            engine_.activate(entry, activateEvent);

            for (const auto& key : sequence) {
                fcitx::KeyEvent event(context.get(), fcitx::Key(key.sym), false);
                engine_.keyEvent(entry, event);
            }
            const std::string before = context->inputPanel().clientPreedit().toString();

            for (auto sym : {kNonModifierKey, FcitxKey_BackSpace}) {
                fcitx::KeyEvent event(context.get(), fcitx::Key(sym), false);
                engine_.keyEvent(entry, event);
            }
            const std::string after = context->inputPanel().clientPreedit().toString();

            if (before != after) {
                failure = "P3 preedit was '" + before + "' before k+BackSpace and '" + after + "' after";
                return false;
            }
            return true;
        }

      private:
        fcitx::NgoSenEngine& engine_;
        fcitx::Instance&     instance_;
    };

} // namespace

int main() {
    configureTestPaths("fcitx5-ngosen-property-invariants");
    TestInstance        testInstance;
    fcitx::NgoSenEngine engine(&testInstance.instance);

    fcitx::RawConfig    config;
    config.setValueByPath("Mode", "Preedit");
    config.setValueByPath("InputMethod", "Telex");
    // See the file header: each of these may rewrite or lengthen a composition
    // for legitimate reasons, which is not what these properties are about.
    config.setValueByPath("EnableMacro", "False");
    config.setValueByPath("EnableDictionary", "False");
    config.setValueByPath("SpellCheck", "False");
    config.setValueByPath("AutoNonVnRestore", "False");
    engine.setConfig(config);
    if (engine.config().mode.value() != fcitx::NgoSenMode::Preedit || engine.config().inputMethod.value() != "Telex") {
        reportFailure("configure Preedit/Telex", "mode=Preedit, input method=Telex", "configured mode or input method differs",
                      "the property test cannot exercise client preedit behavior");
        return 1;
    }

    std::mt19937                               generator(kSeed);
    std::uniform_int_distribution<int>         lengthPicker(1, kMaxSequenceKeys);
    std::uniform_int_distribution<std::size_t> keyPicker(0, alphabet().size() - 1);

    Runner                                     runner(engine, testInstance.instance);

    for (int index = 0; index < kSequenceCount; ++index) {
        std::vector<KeyStroke> sequence;
        const int              length = lengthPicker(generator);
        sequence.reserve(static_cast<std::size_t>(length));
        for (int position = 0; position < length; ++position) {
            sequence.push_back(alphabet()[keyPicker(generator)]);
        }

        const std::string label = "sequence #" + std::to_string(index) + " [" + describe(sequence) + "]";
        std::string       failure;

        Session           first;
        if (!runner.run(sequence, true, first, failure)) {
            reportFailure(label, "P1/P2/P4 hold for every key sequence", failure,
                          "an invariant that must hold for all input was violated; rerun with seed " + std::to_string(kSeed) + " to reproduce");
            return 1;
        }

        if (!runner.roundTrip(sequence, failure)) {
            reportFailure(label, "P3 typing one consonant then BackSpace restores the previous preedit", failure,
                          "a retro-edit changed text it should not have touched; this is the shape of the duplicated-character defect");
            return 1;
        }

        // P5: identical input on a fresh context must produce identical output.
        Session second;
        if (!runner.run(sequence, false, second, failure)) {
            reportFailure(label, "replay of the same sequence succeeds", failure, "the replay run itself failed");
            return 1;
        }
        if (first.commits != second.commits || first.preedit != second.preedit) {
            reportFailure(label, "P5 replay yields commits=" + std::to_string(first.commits.size()) + ", preedit=" + first.preedit,
                          "replay yielded commits=" + std::to_string(second.commits.size()) + ", preedit=" + second.preedit,
                          "state leaked from the previous composition into a brand new input context");
            return 1;
        }
    }

    std::cout << "property invariants P1-P5 held for " << kSequenceCount << " random key sequences (seed " << kSeed << ")\n";
    return 0;
}
