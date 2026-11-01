/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _FCITX5_NGOSEN_MENU_ICON_H_
#define _FCITX5_NGOSEN_MENU_ICON_H_

#include <string>

namespace fcitx {

    // themed if the icon theme has an icon by exactly that name, otherwise own.
    std::string menuIconName(const std::string& themeName, const std::string& themed, const std::string& own);

    // The same, for the icon theme of the current desktop.
    std::string menuIconName(const std::string& themed, const std::string& own);

} // namespace fcitx

#endif // _FCITX5_NGOSEN_MENU_ICON_H_
