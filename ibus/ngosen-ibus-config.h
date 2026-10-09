/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ngosen-mode.h"
#include "ngosen-options.h"

#include <string>

namespace ngosen {

    struct IBusSettings {
        Options options;
        Mode    mode = Mode::Sen;
    };

    // The fcitx5 version's settings file, so both versions type the same way. Missing keys keep the
    // defaults that version uses; the modes the IBus version cannot run fall back to Sen.
    IBusSettings readSettings(const std::string& path);

    // $XDG_CONFIG_HOME/fcitx5/conf/lotus.conf
    std::string settingsPath();

} // namespace ngosen
