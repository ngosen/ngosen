/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Keys replayed after a wait: a replacement of text not yet sent goes into that text instead of out as a deletion.
#include "fake-app.h"
#include "fake-engine.h"
#include "fake-host.h"

#include "ngosen-globals.h"
#include "ngosen-keysym.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-xtest.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using ngosen::Field;
using ngosen::test::FakeLoop;
using ngosen::test::FakeResources;

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what, const std::string& actual) {
        if (!ok) {
            std::cerr << "FAIL: " << what << "\n  got: " << actual << '\n';
            ++failures;
        }
    }

    std::string join(const std::vector<std::string>& parts) {
        std::string out;
        for (const auto& part : parts)
            out += "['" + part + "']";
        return out.empty() ? "(none)" : out;
    }

    Field makeField(const std::string& frontend, bool surroundingText, const std::string& program = {}) {
        Field field;
        field.frontend        = frontend;
        field.program         = program;
        field.surroundingText = surroundingText;
        field.preedit         = true;
        return field;
    }

    size_t countHaving(const std::vector<std::string>& lines, const std::string& part) {
        return static_cast<size_t>(std::count_if(lines.begin(), lines.end(), [&](const std::string& line) { return line.find(part) != std::string::npos; }));
    }

    std::string after(const std::string& line, const std::string& marker) {
        const auto at = line.find(marker);
        return at == std::string::npos ? std::string() : line.substr(at + marker.size());
    }

    // Replacements that delete only the end of the text the replay committed right before them.
    size_t replacementsOfReplayedText(const std::vector<std::string>& lines) {
        size_t count = 0;
        for (size_t i = 1; i < lines.size(); ++i) {
            const std::string replaced  = after(lines[i], "Perform replacement: ");
            const std::string committed = after(lines[i - 1], "Commit: ");
            if (replaced.empty() || committed.empty())
                continue;
            const std::string deleted = replaced.substr(0, replaced.find(" -> "));
            count += !deleted.empty() && isEndsWith(committed, deleted);
        }
        return count;
    }

    struct Run {
        std::vector<std::string> lines;
        std::string              text;
        int                      backspaces = 0;
    };

    // Keys typed into a Firefox field on wlroots whose reports come late, so keys queue and replay.
    Run typeWithLateReports(bool fold, int laterDelayMs = 250) {
        realMode = ngosen::Mode::Sen;
        realtextLen.store(0);
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().foldReplayReplacement = fold;
        auto owned                                       = std::make_unique<ngosen::test::FakeApp>(
            loop, makeField("wayland_v2", true, "firefox"),
            ngosen::test::AppQuirks{.reportDelayMs = 30, .backspaceLagMs = 60, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true});
        auto*               app = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        auto typeAt = [&](const std::string& typed, int gapMs) {
            for (const char c : typed) {
                app->type(static_cast<uint32_t>(c));
                loop.pump(gapMs);
            }
            loop.pump(400);
        };
        typeAt("tieengs ", 150);
        app->setReportDelay(250);
        typeAt("vieetj tieesng ", 150);
        app->setReportDelay(laterDelayMs);
        ngosen::test::captureLog(true);
        typeAt("nguwx cuar ddeepj nguwowif tieengs vieetj ", 60);
        loop.pump(5000);
        Run run{ngosen::test::capturedLog(), app->text(), app->sent().forwardedBackSpaces};
        ngosen::test::captureLog(false);
        return run;
    }

    // Types after a warm-up word, waits until every queued key has gone out, and keeps the log of the typed part.
    Run typed(const Field& field, ngosen::test::AppQuirks quirks, const std::string& text, int gapMs, bool fold) {
        realMode = ngosen::Mode::Sen;
        realtextLen.store(0);
        FakeLoop      loop;
        FakeResources resources;
        resources.mutableOptions().foldReplayReplacement = fold;
        auto                owned                        = std::make_unique<ngosen::test::FakeApp>(loop, field, quirks);
        auto*               app                          = owned.get();
        ngosen::TypingState state(&resources, std::move(owned));
        app->attach(&state);
        for (const char c : std::string("tieengs ")) {
            app->type(static_cast<uint32_t>(c));
            loop.pump(150);
        }
        loop.pump(1000);
        ngosen::test::captureLog(true);
        for (const char c : text) {
            app->type(c == '\n' ? ngosen::key::Return : static_cast<uint32_t>(c));
            loop.pump(gapMs);
        }
        loop.pump(5000);
        Run run{ngosen::test::capturedLog(), app->text(), app->sent().forwardedBackSpaces};
        ngosen::test::captureLog(false);
        return run;
    }

    void testReplayFoldsReplacement() {
        const Run on  = typeWithLateReports(true);
        const Run off = typeWithLateReports(false);
        std::cerr << "fold on: replay replacements " << replacementsOfReplayedText(on.lines) << ", folds " << countHaving(on.lines, "Fold replacement into the replay")
                  << ", backspaces " << on.backspaces << "\nfold off: replay replacements " << replacementsOfReplayedText(off.lines) << ", folds "
                  << countHaving(off.lines, "Fold replacement into the replay") << ", backspaces " << off.backspaces << '\n';
        const std::string want = "tiếng việt tiếng ngữ của đệp người tiếng việt ";
        check(on.text == want, "fold: the words come out right", on.text);
        check(off.text == want, "fold off: the words come out right", off.text);
        check(replacementsOfReplayedText(on.lines) == 0, "fold: no replacement deletes text the replay just sent", std::to_string(replacementsOfReplayedText(on.lines)));
        check(countHaving(on.lines, "Fold replacement into the replay") >= 1, "fold: replacements go into the replayed text", "none");
        check(on.backspaces < off.backspaces, "fold: fewer backspaces than without it", std::to_string(on.backspaces) + " vs " + std::to_string(off.backspaces));
        check(countHaving(off.lines, "Fold replacement into the replay") == 0, "fold off: nothing folded", "some");
    }

    const ngosen::test::AppQuirks lateFirefox{.reportDelayMs = 250, .backspaceLagMs = 60, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true};
    const ngosen::test::AppQuirks lateXTest{.lagMs = 60, .takesXTest = true};

    size_t                        folds(const Run& run) {
        return countHaving(run.lines, "Fold replacement into the replay");
    }

    // The restore of an English-looking word at the space goes into the replay too.
    void testReplayFoldAtWordEnd() {
        const Field field = makeField("wayland_v2", true, "firefox");
        const Run   on    = typed(field, lateFirefox, "nguwx tunx nguwx tunx ", 40, true);
        const Run   off   = typed(field, lateFirefox, "nguwx tunx nguwx tunx ", 40, false);
        const bool  atEnd = std::any_of(on.lines.begin(), on.lines.end(),
                                        [](const std::string& line) { return line.find("Fold replacement into the replay") != std::string::npos && isEndsWith(line, "unx "); });
        check(atEnd, "word end: the restored word is folded", join(on.lines));
        check(replacementsOfReplayedText(on.lines) == 0, "word end: nothing replayed is deleted", std::to_string(replacementsOfReplayedText(on.lines)));
        check(replacementsOfReplayedText(off.lines) > 0, "word end, fold off: replayed text is deleted", "0");
        check(on.text == "tiếng ngữ tunx ngữ tunx " && off.text == on.text, "word end: the words come out right", on.text + " / " + off.text);
    }

    void testReplayFoldPasswordHidden() {
        Field field    = makeField("wayland_v2", true, "firefox");
        field.password = true;
        const Run on   = typed(field, lateFirefox, "nguwx cuar nguwowif ", 40, true);
        check(folds(on) >= 1 && folds(on) == countHaving(on.lines, "Fold replacement into the replay: (hidden) -> (hidden)"), "password: folds log no text", join(on.lines));
        check(countHaving(on.lines, "ngữ") == 0 && countHaving(on.lines, "của") == 0, "password: the typed words stay out of the log", join(on.lines));
        check(replacementsOfReplayedText(on.lines) == 0, "password: nothing replayed is deleted", std::to_string(replacementsOfReplayedText(on.lines)));
    }

    // XIM and X11 fields take XTEST backspaces in order with commits; a fold sends fewer of them.
    void testReplayFoldXTest() {
        for (const char* frontend : {"xim", "x11"}) {
            const std::string name  = frontend;
            const Field       field = makeField(frontend, false);
            const Run         on    = typed(field, lateXTest, "nguwx cuar nguwowif ", 40, true);
            const Run         off   = typed(field, lateXTest, "nguwx cuar nguwowif ", 40, false);
            check(folds(on) >= 1 && replacementsOfReplayedText(on.lines) == 0, name + ": the replacement is folded", join(on.lines));
            check(replacementsOfReplayedText(off.lines) >= 1, name + ", fold off: replayed text is deleted", "0");
            check(on.text == "tiếng ngữ của người " && off.text == on.text, name + ": the words come out right", on.text + " / " + off.text);
        }
    }

    // An Enter queued behind a folded word goes out once, after it.
    void testReplayFoldBeforeEnter() {
        for (const char* frontend : {"xim", "x11"}) {
            const std::string name = frontend;
            const Run         on   = typed(makeField(frontend, false), lateXTest, "nguwx cuar tunx\nnguwx tunx\n", 40, true);
            const Run         off  = typed(makeField(frontend, false), lateXTest, "nguwx cuar tunx\nnguwx tunx\n", 40, false);
            check(folds(on) >= 1, name + " Enter: a replacement is folded", join(on.lines));
            check(on.text == "tiếng ngữ của tũn\rngữ tũn\r" && off.text == on.text, name + " Enter: each Enter once, after its word", on.text + " / " + off.text);
        }
    }

    // GNOME Shell's bridge keeps one edit per turn, so a deletion right after a replayed commit can take the wrong text.
    void testReplayFoldOnGnomeShell() {
        Field field       = makeField("ibus", true, "gnome-shell");
        field.sendsNoKeys = true;
        const ngosen::test::AppQuirks shell{
            .reportDelayMs = 250, .dropsForwardedKeys = true, .convertsDeleteOnReportedCopy = true, .reportsTextAtChange = true, .keepsLastEditPerTurn = true, .turnMs = 3};
        const Run on = typed(field, shell, "nguwx cuar nguwowif ", 40, true);
        check(folds(on) >= 1, "GNOME Shell: a replacement is folded", join(on.lines));
        check(on.text == "tiếng ngữ của người ", "GNOME Shell: the words come out right", on.text);
    }

} // namespace

int main() {
    setXTestSenderForTest([](int) { return true; });
    testReplayFoldsReplacement();
    testReplayFoldAtWordEnd();
    testReplayFoldPasswordHidden();
    testReplayFoldXTest();
    testReplayFoldBeforeEnter();
    testReplayFoldOnGnomeShell();
    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "core-replay-fold: all passed\n";
    return 0;
}
