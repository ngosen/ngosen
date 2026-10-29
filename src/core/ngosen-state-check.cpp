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

    void TypingState::openSurroundingCheck(const Surrounding& sent, const std::string& retype) {
        if (surrCheck_) {
            surr_check_timer_.reset();
            closeSurroundingCheck(SurroundingVerdict::NoAnswer, true);
        }
        surrCheck_        = SurroundingCheck{.prefix     = surr_wait_prefix_,
                                             .deleted    = surr_wait_deleted_,
                                             .added      = pending_commit_string_,
                                             .retype     = retype,
                                             .sentReport = reportKey(sent),
                                             .password   = host_->field().password,
                                             .keysAtSend = keyCount_,
                                             .sentAtUs   = monotonicUs()};
        const int waitMs  = std::max(engine_->options().waitSurroundingTimeoutMs, SurroundingCheckWaitMs);
        surr_check_timer_ = host_->startTimer(monotonicUs() + static_cast<uint64_t>(waitMs) * 1000ULL, 0, [this](Timer&) {
            // The timer is still running here, so closing must not reset it.
            if (surrCheck_)
                closeSurroundingCheck(SurroundingVerdict::NoAnswer);
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
        const auto verdict = judgeOpenCheck(host_->surrounding());
        // A deletion and its commit can reach the app as two changes; wait for the commit.
        if (verdict == SurroundingVerdict::NoAnswer)
            return;
        surr_check_timer_.reset();
        closeSurroundingCheck(verdict);
    }

    void TypingState::closeSurroundingCheck(SurroundingVerdict verdict, bool beforeNext) {
        const auto check = *surrCheck_;
        surrCheck_.reset();
        std::string line = std::string("Surrounding check: ") + verdictName(verdict) + (beforeNext ? " before the next replacement" : "") + " after " +
            std::to_string((monotonicUs() - check.sentAtUs) / 1000) + " ms, " + std::to_string(keyCount_ - check.keysAtSend) + " keys later" + loggedField(host_->field());
        if (!check.password)
            line += ", replaced " + check.deleted + " -> " + check.added;
        if (verdict == SurroundingVerdict::Correct)
            surrAnswered_ = true;
        surrUnanswered_ = verdict == SurroundingVerdict::NoAnswer ? surrUnanswered_ + 1 : 0;
        if (verdict == SurroundingVerdict::DroppedDeletion || verdict == SurroundingVerdict::WrongEarlierPart || (!surrAnswered_ && surrUnanswered_ >= UnansweredLimit))
            line += "; would stop trusting this field";
        NGOSEN_INFO(line);
    }

} // namespace ngosen
