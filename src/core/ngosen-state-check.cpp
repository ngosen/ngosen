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
        // Unanswered checks in a row, with none correct, that would make the field untrusted.
        constexpr int UnansweredLimit = 3;
    } // namespace

    bool TypingState::deletesOnlyThroughSurrounding() const {
        // LibreOffice runs Backspace as an async shortcut, so committed text overtakes it. Its
        // deleteSurroundingText applies at once, relative to the cursor, so use it there (#162).
        // A Backspace on a Calc cell that is not being edited opens the Delete Contents dialog.
        const Field field         = host_->field();
        const bool  isLibreOffice = ngosen::appliesBackspacesLate(field) || ngosen::reportsFieldOnlyOnKey(field);
        return field.surroundingText && (isLibreOffice || dropsForwardedKeys() || field.sendsNoKeys);
    }

    bool TypingState::surroundingUntrusted() const {
        const Field field = host_->field();
        return autofillMarked_ || engine_->surroundingUntrusted(field.frontend, field.program);
    }

    bool TypingState::stopsReplacing() const {
        const Field field = host_->field();
        return !field.preeditFallback && deletesOnlyThroughSurrounding() && engine_->surroundingUntrusted(field.frontend, field.program);
    }

    bool TypingState::canReplaceInApp() const {
        return (canSendBackspaces() || host_->field().surroundingText) && !stopsReplacing() && !rawWord_;
    }

    void TypingState::markSurroundingUntrusted(const std::string& why) {
        const Field field = host_->field();
        if (engine_->surroundingUntrusted(field.frontend, field.program))
            return;
        engine_->markSurroundingUntrusted(field.frontend, field.program);
        const char* next = !deletesOnlyThroughSurrounding() ? "delete with backspaces" : field.preeditFallback ? "type words in the preedit" : "type keys as they are";
        NGOSEN_INFO("Surrounding text untrusted" + loggedField(field) + ": " + why + "; from now " + next);
    }

    void TypingState::openSurroundingCheck(const Surrounding& sent, const std::string& retype) {
        if (surrCheck_) {
            surr_check_timer_.reset();
            closeSurroundingCheck(SurroundingVerdict::NoAnswer, nullptr, true);
        }
        const std::string& text = sent.text();
        surrCheck_              = SurroundingCheck{.prefix        = surr_wait_prefix_,
                                                   .deleted       = surr_wait_deleted_,
                                                   .added         = pending_commit_string_,
                                                   .retype        = retype,
                                                   .sentReport    = reportKey(sent),
                                                   .sentAfter     = sent.isValid() ? text.substr(textBeforeCursor(sent).size()) : std::string(),
                                                   .password      = host_->field().password,
                                                   .keysAtSend    = keyCount_,
                                                   .commitsAtSend = commitCount_,
                                                   .sentAtUs      = monotonicUs()};
        const int waitMs        = std::max(engine_->options().waitSurroundingTimeoutMs, SurroundingCheckWaitMs);
        surr_check_timer_       = host_->startTimer(monotonicUs() + static_cast<uint64_t>(waitMs) * 1000ULL, 0, [this](Timer&) {
            // The timer is still running here, so closing must not reset it.
            if (surrCheck_)
                closeSurroundingCheck(SurroundingVerdict::NoAnswer, nullptr);
            return false;
        });
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
        if (!check.password)
            line += ", replaced " + check.deleted + " -> " + check.added;
        NGOSEN_INFO(line);
        if (verdict == SurroundingVerdict::Correct)
            surrAnswered_ = true;
        surrUnanswered_ = verdict == SurroundingVerdict::NoAnswer ? surrUnanswered_ + 1 : 0;
        // Calc reports an empty field and GTK 4 an old copy as a matter of course, so with no other way
        // to delete only a dropped deletion is worth giving up on the field for.
        const bool onlyWay = deletesOnlyThroughSurrounding() && !host_->field().preeditFallback;
        if (verdict == SurroundingVerdict::DroppedDeletion) {
            markSurroundingUntrusted("the app dropped a deletion");
            if (report != nullptr && !deletesOnlyThroughSurrounding())
                fixDroppedDeletion(check, *report);
        } else if (verdict == SurroundingVerdict::WrongEarlierPart && !onlyWay) {
            markSurroundingUntrusted("the text before a commit changed");
        } else if (!surrAnswered_ && surrUnanswered_ >= UnansweredLimit && !onlyWay) {
            markSurroundingUntrusted(std::to_string(surrUnanswered_) + " deletions in a row went unanswered");
        }
    }

    void TypingState::fixDroppedDeletion(const SurroundingCheck& check, const Surrounding& report) {
        // Backspaces would land on whatever came after, so fix only text that is exactly what we left.
        const std::string left      = check.prefix + check.deleted + check.added;
        const std::string before    = textBeforeCursor(report);
        const bool        exact     = report.cursor() == report.anchor() && isEndsWith(before, left) && report.text().substr(before.size()) == check.sentAfter;
        const bool        untouched = keyCount_ == check.keysAtSend && commitCount_ == check.commitsAtSend;
        // With no prefix the text before the word may only look like the dropped part. Unmarked, the
        // fix would go through the surrounding text again and be dropped again.
        if (!surroundingUntrusted() || !exact || !untouched || check.prefix.empty() || !check.retype.empty() || is_deleting_.load(std::memory_order_acquire)) {
            NGOSEN_INFO("Leave the dropped deletion as it is");
            return;
        }
        const std::string extra = check.deleted + check.added;
        NGOSEN_INFO("Fix the dropped deletion with " + std::to_string(utf8::length(extra)) + " backspaces");
        commitNotSeen_.clear();
        pending_commit_string_         = check.added;
        expected_backspaces_           = static_cast<int>(utf8::length(extra));
        deletedChars_                  = expected_backspaces_;
        current_backspace_count_       = 0;
        surr_wait_prefix_              = check.prefix;
        surr_wait_deleted_             = extra;
        surr_wait_sent_snapshot_       = reportKey(report);
        surr_wait_sent_snapshot_fresh_ = true;
        deleteAndCommit(extra, check.added);
    }

} // namespace ngosen
