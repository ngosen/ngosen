/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "ngosen-menu-icon.h"

#include <fcitx/icontheme.h>
#include <filesystem>

namespace fcitx {

    std::string menuIconName(const std::string& themeName, const std::string& themed, const std::string& own) {
        const IconTheme theme(themeName);
#if NGOSEN_USE_MODERN_FCITX_API
        const std::filesystem::path found = theme.findIconPath(themed, 16);
#else
        const std::filesystem::path found = theme.findIcon(themed, 16);
#endif
        // The lookup drops trailing "-word" parts when nothing matches, so "format-text" would count.
        return found.stem() == themed ? themed : own;
    }

    std::string menuIconName(const std::string& themed, const std::string& own) {
        static const std::string themeName = IconTheme::defaultIconThemeName();
        return menuIconName(themeName, themed, own);
    }

} // namespace fcitx
