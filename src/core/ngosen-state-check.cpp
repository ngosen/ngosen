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

#include <string>

namespace ngosen {

    namespace {
        // Apps that lag report within LaggingReportWaitMs; twice that leaves room for a slow one.
        constexpr int SurroundingCheckWaitMs = 200;
        // Unanswered checks in a row, with none correct, that would make the field untrusted.
        constexpr int UnansweredLimit = 3;

        std::string   reportOf(const Surrounding& s) {
            return s.isValid() ? s.text() + "\x1f" + std::to_string(s.cursor()) : std::string();
        }
    } // namespace

    void TypingState::openSurroundingCheck(const Surrounding& sent) {
        if (surrCheck_)
            closeSurroundingCheck(SurroundingVerdict::NoAnswer);
        surrCheck_        = SurroundingCheck{.prefix     = surr_wait_prefix_,
                                             .deleted    = surr_wait_deleted_,
                                             .added      = pending_commit_string_,
                                             .sentReport = reportOf(sent),
                                             .keysAtSend = keyCount_,
                                             .sentAtUs   = monotonicUs()};
        surr_check_timer_ = host_->startTimer(monotonicUs() + SurroundingCheckWaitMs * 1000ULL, 0, [this](Timer&) {
            // The timer is still running here, so closing must not reset it.
            if (surrCheck_)
                closeSurroundingCheck(SurroundingVerdict::NoAnswer);
            return false;
        });
    }

    void TypingState::checkSurroundingReport() {
        const Surrounding s = host_->surrounding();
        if (!surrCheck_ || !s.isValid() || reportOf(s) == surrCheck_->sentReport)
            return;
        const std::string word       = surrCheck_->prefix + surrCheck_->added;
        const std::string typedAfter = isStartsWith(oldPreBuffer_, word) ? oldPreBuffer_.substr(word.size()) : std::string();
        const auto        verdict    = judgeSurroundingReport(textBeforeCursor(s), surrCheck_->prefix, surrCheck_->deleted, surrCheck_->added, typedAfter);
        // A deletion and its commit can reach the app as two changes; wait for the commit.
        if (verdict == SurroundingVerdict::NoAnswer)
            return;
        surr_check_timer_.reset();
        closeSurroundingCheck(verdict);
    }

    void TypingState::closeSurroundingCheck(SurroundingVerdict verdict) {
        const auto check = *surrCheck_;
        surrCheck_.reset();
        const bool  password = host_->field().password;
        std::string line     = std::string("Surrounding check: ") + verdictName(verdict) + " after " + std::to_string((monotonicUs() - check.sentAtUs) / 1000) + " ms, " +
            std::to_string(keyCount_ - check.keysAtSend) + " keys later" + loggedField(host_->field());
        if (!password)
            line += ", replaced " + check.deleted + " -> " + check.added;
        if (verdict == SurroundingVerdict::Correct)
            surrAnswered_ = true;
        surrUnanswered_ = verdict == SurroundingVerdict::NoAnswer ? surrUnanswered_ + 1 : 0;
        if (verdict == SurroundingVerdict::DroppedDeletion || verdict == SurroundingVerdict::WrongEarlierPart || (!surrAnswered_ && surrUnanswered_ >= UnansweredLimit))
            line += "; would stop trusting this field";
        NGOSEN_INFO(line);
    }

} // namespace ngosen
