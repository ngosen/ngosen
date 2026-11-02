/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <thread>

namespace ngosen {

    // Microseconds of a clock tests move by hand so timers fire without waiting; 0 means the real clock.
    inline uint64_t fakeClockUs = 0;

    // Microseconds on CLOCK_MONOTONIC, the clock Host::startTimer deadlines are on.
    inline uint64_t monotonicUs() {
        if (fakeClockUs != 0)
            return fakeClockUs;
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
    }

    inline void sleepUs(uint64_t us) {
        if (fakeClockUs != 0)
            fakeClockUs += us;
        else
            std::this_thread::sleep_for(std::chrono::microseconds(us));
    }

} // namespace ngosen
