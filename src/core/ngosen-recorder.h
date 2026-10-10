/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-host.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>

namespace ngosen {

    // The last few hundred typing events, kept in memory only, so a user who just saw a wrong word
    // can save what led to it instead of trying to reproduce it.
    class Recorder {
      public:
        explicit Recorder(size_t capacity = 400) : capacity_(capacity) {}

        void add(const std::string& kind, const std::string& detail);
        // One event per line: milliseconds since the first kept event, kind and detail, tab separated.
        std::string dump() const;
        void        clear() {
            events_.clear();
            droppedField_.reset();
        }

      private:
        struct Event {
            uint64_t    timeUs;
            std::string kind;
            std::string detail;
        };

        size_t            capacity_;
        std::deque<Event> events_;
        // The kept keys are useless for replay without the field they were typed into.
        std::optional<Event> droppedField_;
    };

    // The text around the cursor, cut to what explains a typing error: the whole field may be a long
    // private document.
    std::string describeSurrounding(const Surrounding& s);

    // Passes every call on to the app's host and records what changes the app's text.
    std::unique_ptr<Host> recordingHost(std::unique_ptr<Host> host, Recorder& recorder);

} // namespace ngosen
