/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>

namespace ngosen {

    // The name app checks match: for a Flatpak app ID with an exported desktop file, the name the core
    // rule (ngosen-flatpak-names.h) gives it; otherwise the name as reported.
    const std::string& normalizeProgramName(const std::string& program);

} // namespace ngosen
