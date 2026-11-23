/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// TypingState: reading back what the app did with a deletion through its surrounding text.
#include "ngosen-app-quirks.h"
#include "ngosen-clock.h"
#include "ngosen-log.h"
#include "ngosen-state.h"
#include "ngosen-strings.h"
#include "ngosen-utf8.h"

#include <algorithm>
#include <string>

namespace ngosen {

    namespace {
        // Apps that lag report within LaggingReportWaitMs; twice that leaves room for a slow one.
        constexpr int SurroundingCheckWaitMs = 200;
        // Covers the gap to the next key at most typing speeds.
        constexpr int LaterKeyCheckWaitMs = 1000;
        // Unanswered checks in a row, with none correct, that would make the field untrusted.
        constexpr int UnansweredLimit = 3;
        // How late a starved app can apply backspaces whose wait ran out.
        constexpr uint64_t LateBackspacesUs = 1000000;
    } // namespace

    bool TypingState::deletesOnlyThroughSurrounding() const {
        // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it. Its
        // deleteSurroundingText applies at once, relative to the cursor, so use it there (#162).
        // A Backspace on a Calc cell that is not being edited opens the Delete Contents dialog.
        const Field field         = host_->field();
        const bool  isLibreOffice = ngosen::appliesBackspacesLate(field) || ngosen::reportsFieldOnlyOnKey(field);
        return field.surroundingText && (isLibreOffice || dropsForwardedKeys() || field.sendsNoKeys);
    }

    // Under one shared name a mark would spread to every app. Firefox's snap goes wrong only while it is
    // busy after opening, and GNOME Shell drops an app's edits only while the machine is busy, so a mark
    // for the app would outlast that.
    bool TypingState::marksFieldOnly() const {
        const Field field = host_->field();
        return ngosen::namesNoApp(field) || field.relayedByShell || ngosen::appliesDeletionsEarly(field);
    }

    // Right after login GNOME Shell can relay every app under its own name and drop every deletion (#816).
    // Backspaces work for all of them, so remembering the drop for the shared name costs nothing.
    bool TypingState::keepsDropForSharedName() const {
        const Field field = host_->field();
        return ngosen::namesNoApp(field) && !ngosen::appliesDeletionsEarly(field) && !deletesOnlyThroughSurrounding();
    }

    bool TypingState::appMarkedUntrusted() const {
        const Field field     = host_->field();
        const bool  appMarked = engine_->surroundingUntrusted(field.frontend, field.program);
        if (!marksFieldOnly())
            return appMarked;
        return sinceFocus_.untrusted || (appMarked && keepsDropForSharedName());
    }

    bool TypingState::surroundingUntrusted() const {
        return sinceFocus_.autofillMarked || appMarkedUntrusted();
    }

    bool TypingState::stopsReplacing() const {
        return !host_->field().preeditFallback && deletesOnlyThroughSurrounding() && appMarkedUntrusted();
    }

    bool TypingState::canReplaceInApp() const {
        return (canSendBackspaces() || host_->field().surroundingText) && !stopsReplacing() && !rawWord_;
    }

    void TypingState::markSurroundingUntrusted(const std::string& why, bool dropped) {
        const Field field = host_->field();
        if (appMarkedUntrusted())
            return;
        if (marksFieldOnly())
            sinceFocus_.untrusted = true;
        if (!marksFieldOnly() || (dropped && keepsDropForSharedName()))
            engine_->markSurroundingUntrusted(field.frontend, field.program);
        const char* next = !deletesOnlyThroughSurrounding() ? "delete with backspaces" : field.preeditFallback ? "type words in the preedit" : "type keys as they are";
        NGOSEN_INFO("Surrounding text untrusted" + loggedField(field) + ": " + why + "; from now " + next);
    }

    // A field with another way to delete can afford to wait: a word that finds the copy stale uses it.
    bool TypingState::checkWaitsForLaterKey() const {
        return !deletesOnlyThroughSurrounding() && ngosen::reportsReplacementsWithLaterKey(host_->field());
    }

    void TypingState::openSurroundingCheck(const Surrounding& sent, const std::string& retype) {
        if (surrCheck_) {
            surr_check_timer_.reset();
            closeSurroundingCheck(SurroundingVerdict::NoAnswer, nullptr, true);
        }
        const std::string& text = sent.text();
        surrCheck_              = SurroundingCheck{.prefix        = surr_wait_.prefix,
                                                   .deleted       = surr_wait_.deleted,
                                                   .added         = pending_commit_string_,
                                                   .retype        = retype,
                                                   .sentReport    = reportKey(sent),
                                                   .sentAfter     = sent.isValid() ? text.substr(textBeforeCursor(sent).size()) : std::string(),
                                                   .program       = host_->field().program,
                                                   .hidden        = host_->field().hidesText(),
                                                   .waitsForKey   = checkWaitsForLaterKey(),
                                                   .keysAtSend    = keyCount_,
                                                   .commitsAtSend = commitCount_,
                                                   .sentAtUs      = monotonicUs()};
        const bool waitsForKey  = surrCheck_->waitsForKey;
        const int  waitMs       = std::max(engine_->options().waitSurroundingTimeoutMs, waitsForKey ? LaterKeyCheckWaitMs : SurroundingCheckWaitMs);
        surr_check_timer_       = host_->startTimer(monotonicUs() + static_cast<uint64_t>(waitMs) * 1000ULL, waitsForKey ? 1000 : 0, [this](Timer&) {
            // The timer is still running here, so closing must not reset it.
            if (surrCheck_)
                closeSurroundingCheck(SurroundingVerdict::NoAnswer, nullptr);
            return false;
        });
    }

    void TypingState::closeCheckBeforeOtherEdit() {
        // The user's Backspace or a replacement through backspaces also changes what stands before the
        // commit, so later reports prove nothing.
        if (!surrCheck_ || !surrCheck_->waitsForKey)
            return;
        surr_check_timer_.reset();
        closeSurroundingCheck(SurroundingVerdict::NoAnswer, nullptr);
    }

    SurroundingVerdict TypingState::judgeOpenCheck(const Surrounding& report) const {
        if (!report.isValid() || reportKey(report) == surrCheck_->sentReport)
            return SurroundingVerdict::NoAnswer;
        // A dropped deletion leaves the old part with the retyped text and the new part after it.
        return judgeSurroundingReport(textBeforeCursor(report), surrCheck_->prefix, surrCheck_->deleted + surrCheck_->retype, surrCheck_->added,
                                      static_cast<unsigned int>(keyCount_ - surrCheck_->keysAtSend));
    }

    void TypingState::checkSurroundingReport() {
        if (!surrCheck_)
            return;
        // Backspaces fixing a dropped deletion would land but their commit would not.
        if (surrCheck_->waitsForKey && !host_->hasFocus())
            return;
        const Surrounding report  = host_->surrounding();
        const auto        verdict = judgeOpenCheck(report);
        // A deletion and its commit can reach the app as two changes; wait for the commit.
        if (verdict == SurroundingVerdict::NoAnswer)
            return;
        surr_check_timer_.reset();
        closeSurroundingCheck(verdict, &report);
    }

    void TypingState::closeSurroundingCheck(SurroundingVerdict verdict, const Surrounding* report, bool beforeNext) {
        const auto check = *surrCheck_;
        surrCheck_.reset();
        std::string line = std::string("Surrounding check: ") + verdictName(verdict) + (beforeNext ? " before the next replacement" : "") + " after " +
            std::to_string((monotonicUs() - check.sentAtUs) / 1000) + " ms, " + std::to_string(keyCount_ - check.keysAtSend) + " keys later" + loggedField(host_->field());
        if (!check.hidden)
            line += ", replaced " + check.deleted + " -> " + check.added;
        NGOSEN_INFO(line);
        if (verdict == SurroundingVerdict::Correct)
            sinceFocus_.checkAnswered = true;
        if (verdict == SurroundingVerdict::Correct && keyCount_ == check.keysAtSend)
            ++sinceFocus_.answeredUnasked;
        else if (verdict == SurroundingVerdict::NoAnswer)
            ++sinceFocus_.checksMissed;
        // An answer after later keys still shows the field keeps up; only the copy wait counts it.
        if (verdict == SurroundingVerdict::Correct || verdict == SurroundingVerdict::NoAnswer) {
            sinceFocus_.recentAnswers <<= 1;
            sinceFocus_.recentAnswers[0] = verdict == SurroundingVerdict::Correct;
            sinceFocus_.recentChecks     = std::min(sinceFocus_.recentChecks + 1, RecentChecks);
        }
        // GNOME Shell can name the app after its field got focus, and the mark is kept per app name.
        if (check.program != host_->field().program) {
            surrUnanswered_ = 0;
            NGOSEN_INFO("Surrounding check spans an app name change: no mark");
            return;
        }
        surrUnanswered_ = verdict == SurroundingVerdict::NoAnswer ? surrUnanswered_ + 1 : 0;
        // Our own backspaces landing late change the text the same way, so they prove nothing about the app.
        const bool dropped   = verdict == SurroundingVerdict::DroppedDeletion || verdict == SurroundingVerdict::ShortDeletion;
        const bool wrongText = dropped || verdict == SurroundingVerdict::WrongEarlierPart;
        if (wrongText && unconfirmedBackspacesUs_ != 0 && monotonicUs() - unconfirmedBackspacesUs_ < LateBackspacesUs) {
            NGOSEN_INFO("Backspaces sent before may still land: no mark");
            return;
        }
        // Calc reports an empty field and GTK 4 an old copy as a matter of course, so with no other way
        // to delete only a dropped deletion is worth giving up on the field for.
        const bool onlyWay = deletesOnlyThroughSurrounding() && !host_->field().preeditFallback;
        // Firefox's snap answers late while it is busy, and backspaces there can land after the next commit.
        const bool answersLate = ngosen::appliesDeletionsEarly(host_->field());
        if (dropped) {
            markSurroundingUntrusted(verdict == SurroundingVerdict::DroppedDeletion ? "the app dropped a deletion" : "the app deleted too little", true);
            if (report != nullptr && !deletesOnlyThroughSurrounding())
                fixDroppedDeletion(check, *report);
        } else if (verdict == SurroundingVerdict::WrongEarlierPart && !onlyWay) {
            markSurroundingUntrusted("the text before a commit changed");
        } else if (!sinceFocus_.checkAnswered && surrUnanswered_ >= UnansweredLimit && !onlyWay && !checkWaitsForLaterKey() && !answersLate) {
            markSurroundingUntrusted(std::to_string(surrUnanswered_) + " deletions in a row went unanswered");
        }
    }

    void TypingState::fixDroppedDeletion(const SurroundingCheck& check, const Surrounding& report) {
        // Backspaces would land on whatever came after, so fix only text that is exactly what we left.
        const std::string before    = textBeforeCursor(report);
        const std::string leftOver  = deletionLeftOver(before, check.prefix, check.deleted, check.added);
        const bool        exact     = report.cursor() == report.anchor() && !leftOver.empty() && report.text().substr(before.size()) == check.sentAfter;
        const bool        untouched = keyCount_ == check.keysAtSend && commitCount_ == check.commitsAtSend;
        // With no prefix the text before the word may only look like the dropped part. Unmarked, the
        // fix would go through the surrounding text again and be dropped again.
        if (!surroundingUntrusted() || !exact || !untouched || check.prefix.empty() || !check.retype.empty() || is_deleting_.load(std::memory_order_acquire)) {
            NGOSEN_INFO("Leave the dropped deletion as it is");
            return;
        }
        const std::string extra = leftOver + check.added;
        NGOSEN_INFO("Fix the dropped deletion with " + std::to_string(utf8::length(extra)) + " backspaces");
        beginReplacement(extra, check.added);
        surr_wait_.prefix            = check.prefix;
        surr_wait_.deleted           = extra;
        surr_wait_.sentSnapshot      = reportKey(report);
        surr_wait_.sentSnapshotFresh = true;
        surr_wait_.leavesText        = showsTextAfterDeleting(report, static_cast<unsigned int>(utf8::length(extra)));
        startReplaceLog("none");
        deleteAndCommit(extra, check.added);
    }

    void TypingState::openBackspaceCheck(const ReplaceRequest& request) {
        const Field       field  = host_->field();
        const std::string before = textBeforeCursor(request.copy);
        // A later report proves something only against a copy that showed the word being replaced.
        if (mode() != ngosen::Mode::Sen || !field.surroundingText || field.hidesText() || !request.copy.isValid() || keepsOnlyCommits() || deferred_commit_.pending ||
            request.deleted.empty() || !isEndsWith(before, request.deleted))
            return;
        backspaceCheck_ = BackspaceCheck{.before = before.substr(0, before.size() - request.deleted.size()), .deleted = request.deleted, .keysAtStart = keyCount_};
    }

    void TypingState::noteBackspaceCommit(const std::string& text) {
        if (!backspaceCheck_ || backspaceCheck_->stage != BackspaceCheck::Stage::Finished || text.empty())
            return;
        backspaceCheck_->added         = text;
        backspaceCheck_->stage         = BackspaceCheck::Stage::Committed;
        backspaceCheck_->committedAtUs = monotonicUs();
        backspace_check_timer_         = host_->startTimer(monotonicUs() + static_cast<uint64_t>(SurroundingCheckWaitMs) * 1000ULL, 0, [this](Timer& t) {
            if (!backspaceCheck_)
                return false;
            // A commit that shows after the usual wait is late, not lost; the log should tell the two apart.
            const uint64_t lastUs = backspaceCheck_->committedAtUs + static_cast<uint64_t>(LaterKeyCheckWaitMs) * 1000ULL;
            if (monotonicUs() + 1000 < lastUs) {
                t.rearm(lastUs);
                return true;
            }
            // The timer is still running here, so closing must not reset it.
            closeBackspaceCheck(false);
            return false;
        });
        // A lost commit changes nothing, so no report may follow it; start from the one that showed the deletion.
        checkBackspaceReport();
    }

    void TypingState::checkBackspaceReport() {
        if (!backspaceCheck_ || backspaceCheck_->stage != BackspaceCheck::Stage::Committed || !host_->hasFocus())
            return;
        auto&             check  = *backspaceCheck_;
        const Surrounding report = host_->surrounding();
        const std::string now    = textBeforeCursor(report);
        // A field that refocuses after edits reports itself empty for a moment (voz.vn in Brave).
        if (!report.isValid() || (now.empty() && !check.before.empty()))
            return;
        const auto keysAfter = static_cast<unsigned int>(keyCount_ - check.keysAtStart);
        check.shown          = judgeSurroundingReport(now, check.before, check.deleted, check.added, keysAfter);
        if (check.shown == SurroundingVerdict::Correct) {
            backspace_check_timer_.reset();
            closeBackspaceCheck(false);
            return;
        }
        // Keys typed since the backspaces may already show after the text.
        check.commitMissing = false;
        std::string shown   = now;
        for (unsigned int dropped = 0; check.shown == SurroundingVerdict::NoAnswer && dropped <= keysAfter; ++dropped) {
            if (check.before.empty() ? shown.empty() : isEndsWith(shown, check.before)) {
                check.commitMissing = true;
                break;
            }
            if (shown.empty())
                break;
            utf8::eraseLastCodepoint(shown);
        }
    }

    void TypingState::closeBackspaceCheck(bool beforeNext) {
        const BackspaceCheck check = *backspaceCheck_;
        backspaceCheck_.reset();
        if (check.stage != BackspaceCheck::Stage::Committed)
            return;
        std::string verdict = "no answer";
        if (check.shown == SurroundingVerdict::Correct)
            verdict = "correct";
        else if (check.commitMissing && !beforeNext)
            // Without text before the word, the deleted state looks like a field that is still redrawing.
            verdict = check.before.empty() ? "unclear" : "missing commit";
        else if (check.shown == SurroundingVerdict::DroppedDeletion || check.shown == SurroundingVerdict::ShortDeletion)
            verdict = "deletion not shown";
        const std::string when = std::to_string((monotonicUs() - check.committedAtUs) / 1000) + " ms";
        engine_->recorder().add("check", verdict + " after " + when);
        NGOSEN_INFO("Backspace check: " + verdict + (beforeNext ? " before the next replacement" : "") + " after " + when + loggedField(host_->field()) + ", replaced " +
                    check.deleted + " -> " + check.added);
    }

    void TypingState::dropBackspaceCheck() {
        backspace_check_timer_.reset();
        backspaceCheck_.reset();
    }

    void TypingState::watchForStrayKey(const ReplaceRequest& request) {
        const Field       field  = host_->field();
        const std::string before = textBeforeCursor(request.copy);
        strayWatch_.reset();
        if (mode() != ngosen::Mode::Sen || !field.surroundingText || field.hidesText() || !request.copy.isValid() || request.copy.cursor() != request.copy.anchor() ||
            request.deleted.empty() || !isEndsWith(before, request.deleted))
            return;
        const std::u32string text = utf8::decode(request.copy.text());
        strayWatch_               = StrayWatch{
                          .before = before.substr(0, before.size() - request.deleted.size()), .after = text.substr(std::min<size_t>(request.copy.cursor(), text.size())), .keys = keyCount_};
    }

    bool TypingState::takesStrayKey(uint32_t sym) {
        constexpr int64_t StrayAfterBounceMs = 300;
        const int64_t     sinceBounce        = static_cast<int64_t>(monotonicUs() / 1000) - lastBounceAt_;
        // Another key in between would leave the order of the two unknown.
        if (!strayWatch_ || strayWatch_->keys != keyCount_ || fieldBounces_ == 0 || sinceBounce > StrayAfterBounceMs) {
            strayWatch_.reset();
            return false;
        }
        strayKey_ = sym;
        return true;
    }

    void TypingState::retypeStrayKey(uint32_t sym, uint32_t states) {
        const StrayWatch     watch  = *strayWatch_;
        const std::string    letter = host_->keyText(sym);
        const Surrounding    s      = host_->surrounding();
        const std::u32string text   = s.isValid() ? utf8::decode(s.text()) : std::u32string();
        const std::string    before = textBeforeCursor(s);
        strayWatch_.reset();
        const auto notRetyped = [&](const std::string& why) {
            engine_->recorder().add("stray", "not retyped: " + why);
            NGOSEN_INFO("Stray key " + logged(letter) + " not retyped" + loggedField(host_->field()) + ": " + why);
        };
        if (!s.isValid() || s.cursor() != s.anchor() || editInFlight() || deferred_commit_.pending)
            return notRetyped("no settled report");
        if (!isStartsWith(before, watch.before) || text.substr(std::min<size_t>(s.cursor(), text.size())) != watch.after)
            return notRetyped("field changed");
        // The letter can only have landed where our backspaces had deleted, or after our commit. It is
        // ASCII, so taking out one byte leaves valid UTF-8.
        const std::string tail  = before.substr(watch.before.size());
        bool              holds = false;
        size_t            kept  = 0;
        for (size_t at = 0; letter.size() == 1 && at < tail.size() && !holds; ++at) {
            const std::string rest = tail.substr(0, at) + tail.substr(at + 1);
            holds                  = tail[at] == letter[0] && isEndsWith(oldPreBuffer_, rest);
            kept                   = oldPreBuffer_.size() - rest.size();
        }
        if (!holds)
            return notRetyped("word is " + logged(tail));
        const std::string shown = oldPreBuffer_.substr(0, kept) + tail;
        // The field changes as after a typed key, so its reports are not a cursor moved by a click.
        lastInputAtUs_ = monotonicUs();
        EngineProcessKeyEvent(bambooEngine_.handle(), sym, states);
        const ngosen::UniqueCPtr<char> preeditC(EnginePullPreedit(bambooEngine_.handle()));
        const std::string              target = preeditC ? preeditC.get() : "";
        std::string                    deletedPart;
        std::string                    addedPart;
        engine_->recorder().add("stray", shown + " -> " + target);
        NGOSEN_INFO("Retype stray key " + logged(letter) + loggedField(host_->field()) + ": " + logged(shown) + " -> " + logged(target));
        if (utf8::compareAndSplitStrings(shown, target, deletedPart, addedPart) != 0) {
            if (deletedPart.empty())
                commit(addedPart);
            else
                performReplacement(shown, deletedPart, addedPart);
        }
        oldPreBuffer_ = target;
    }

} // namespace ngosen
