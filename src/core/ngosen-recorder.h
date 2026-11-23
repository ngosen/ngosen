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

    // The last couple of thousand typing events, kept in memory only, so a user who just saw a wrong word
    // can save what led to it instead of trying to reproduce it.
    class Recorder {
      public:
        // Browsers report the text several times per key: 400 events held only about 7 seconds of
        // typing in a forum composer in Brave, too short to catch the mistake.
        explicit Recorder(size_t capacity = 2000) : capacity_(capacity) {}

        void add(const std::string& kind, const std::string& detail);
        // Adds a field event unless it is the field of the last one.
        void addField(const std::string& detail);
        // Adds what the app reports about its text unless the last event is the same report.
        void addSurrounding(const std::string& detail);
        // One event per line: milliseconds since the first kept event, kind and detail, tab separated.
        std::string dump() const;
        void        clear() {
            events_.clear();
            droppedField_.reset();
            lastField_.clear();
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
        std::string          lastField_;
    };

    // The text around the cursor, cut to what explains a typing error: the whole field may be a long
    // private document.
    std::string describeSurrounding(const Surrounding& s);

    // Passes every call on to the app's host and records what changes the app's text.
    std::unique_ptr<Host> recordingHost(std::unique_ptr<Host> host, Recorder& recorder);

} // namespace ngosen
