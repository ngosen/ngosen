/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <ibus.h>

namespace ngosen {

    // Registers the "ngosen" engine with the factory.
    void addEngine(IBusFactory* factory);

} // namespace ngosen
