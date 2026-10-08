/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <ctime>

namespace ngosen {

    // Microseconds on CLOCK_MONOTONIC, the clock Host::startTimer deadlines are on.
    inline uint64_t monotonicUs() {
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
    }

} // namespace ngosen
