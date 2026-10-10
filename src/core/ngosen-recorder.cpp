/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-recorder.h"

#include "ngosen-clock.h"
#include "ngosen-utf8.h"

#include <cstdio>
#include <utility>
#include <vector>

namespace ngosen {

    namespace {

        constexpr size_t CharsBeforeCursor = 40;
        constexpr size_t CharsAfterCursor  = 10;

        const char*      editKeyName(EditKey key) {
            switch (key) {
                case EditKey::BackSpace: return "BackSpace";
                case EditKey::Right: return "Right";
                case EditKey::Shift: return "Shift";
            }
            return "?";
        }

        class RecordingHost final : public Host {
          public:
            RecordingHost(std::unique_ptr<Host> host, Recorder& recorder) : host_(std::move(host)), recorder_(recorder) {}

            void commitText(const std::string& text) override {
                recorder_.add("commit", quoted(text));
                host_->commitText(text);
            }
            void forwardKey(EditKey key, bool release) override {
                recorder_.add("forward", std::string(editKeyName(key)) + (release ? " up" : " down"));
                host_->forwardKey(key, release);
            }
            void deleteSurrounding(int offset, unsigned int size) override {
                recorder_.add("delete", "offset=" + std::to_string(offset) + " size=" + std::to_string(size));
                host_->deleteSurrounding(offset, size);
            }
            bool pressSystemKeys(int count) override {
                const bool pressed = host_->pressSystemKeys(count);
                recorder_.add("press", "count=" + std::to_string(count) + (pressed ? "" : " failed"));
                return pressed;
            }

            Surrounding surrounding() const override {
                return host_->surrounding();
            }
            Field field() const override {
                return host_->field();
            }
            bool hasFocus() const override {
                return host_->hasFocus();
            }

            void showPreedit(const std::string& text, bool underline) override {
                recorder_.add("preedit", quoted(text));
                host_->showPreedit(text, underline);
            }
            void clearPreedit() override {
                host_->clearPreedit();
            }
            void resetPanel() override {
                host_->resetPanel();
            }
            void refreshPreedit() override {
                host_->refreshPreedit();
            }
            void refreshPanel() override {
                host_->refreshPanel();
            }

            void showCandidates(const std::vector<std::string>& labels, int pageSize, std::function<void(size_t)> onPick) override {
                host_->showCandidates(labels, pageSize, std::move(onPick));
            }
            void hideCandidates() override {
                host_->hideCandidates();
            }
            std::optional<CandidatePage> candidates() const override {
                return host_->candidates();
            }
            void highlightCandidate(int index) override {
                host_->highlightCandidate(index);
            }
            void nextCandidatePage() override {
                host_->nextCandidatePage();
            }
            void prevCandidatePage() override {
                host_->prevCandidatePage();
            }
            void pickCandidate(int index) override {
                host_->pickCandidate(index);
            }
            void setStatus(const std::string& text) override {
                host_->setStatus(text);
            }
            std::string translate(const char* text) const override {
                return host_->translate(text);
            }

            std::unique_ptr<Timer> startTimer(uint64_t deadlineUs, uint64_t accuracyUs, std::function<bool(Timer&)> onTime) override {
                return host_->startTimer(deadlineUs, accuracyUs, std::move(onTime));
            }

            std::string keyText(uint32_t sym) const override {
                return host_->keyText(sym);
            }

          private:
            std::string quoted(const std::string& text) const {
                return host_->field().password ? "hidden" : "\"" + text + "\"";
            }

            std::unique_ptr<Host> host_;
            Recorder&             recorder_;
        };

    } // namespace

    void Recorder::add(const std::string& kind, const std::string& detail) {
        if (capacity_ == 0)
            return;
        if (events_.size() == capacity_) {
            if (events_.front().kind == "field")
                droppedField_ = std::move(events_.front());
            events_.pop_front();
        }
        events_.push_back({monotonicUs(), kind, detail});
    }

    std::string Recorder::dump() const {
        std::string out;
        if (events_.empty())
            return out;
        const uint64_t start = events_.front().timeUs;
        char           ms[32];
        if (droppedField_)
            out += "0.0\tfield\t" + droppedField_->detail + "\n";
        for (const auto& event : events_) {
            std::snprintf(ms, sizeof(ms), "%.1f", static_cast<double>(event.timeUs - start) / 1000.0);
            out += std::string(ms) + "\t" + event.kind + "\t" + event.detail + "\n";
        }
        return out;
    }

    std::string describeSurrounding(const Surrounding& s) {
        if (!s.isValid())
            return "none";
        // Byte offset of every character start, plus the end.
        std::vector<size_t> starts;
        const std::string&  text = s.text();
        for (size_t i = 0; i < text.size();) {
            starts.push_back(i);
            const size_t n = utf8::charBytes(std::string_view(text).substr(i));
            i += n == 0 ? text.size() - i : n;
        }
        starts.push_back(text.size());
        const size_t chars  = starts.size() - 1;
        const size_t cursor = s.cursor() < chars ? s.cursor() : chars;
        const size_t from   = cursor > CharsBeforeCursor ? cursor - CharsBeforeCursor : 0;
        const size_t to     = cursor + CharsAfterCursor < chars ? cursor + CharsAfterCursor : chars;
        std::string  out    = from > 0 ? "…" : "";
        out += text.substr(starts[from], starts[cursor] - starts[from]) + "|" + text.substr(starts[cursor], starts[to] - starts[cursor]);
        if (to < chars)
            out += "…";
        return "\"" + out + "\" cursor=" + std::to_string(s.cursor()) + " anchor=" + std::to_string(s.anchor()) + " length=" + std::to_string(chars);
    }

    std::unique_ptr<Host> recordingHost(std::unique_ptr<Host> host, Recorder& recorder) {
        return std::make_unique<RecordingHost>(std::move(host), recorder);
    }

} // namespace ngosen
