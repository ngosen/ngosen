/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>

namespace ngosen {

    // The typing modes. The config names in lotus-config.h follow this order.
    enum class Mode : std::uint8_t {
        Off,
        Sen,
        Preedit,
        Emoji,
    };

} // namespace ngosen
