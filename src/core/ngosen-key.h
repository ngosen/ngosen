/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <string>

namespace ngosen {

    // A key press or release from the input method framework. Symbols and modifier states use the
    // X11 keysym numbers. A key the typing logic does not accept goes on to the app.
    class KeyPress {
      public:
        virtual ~KeyPress() = default;

        // The key as pressed, before any replaceSym.
        virtual uint32_t sym() const       = 0;
        virtual uint32_t states() const    = 0;
        virtual bool     isRelease() const = 0;
        // The key is itself a modifier such as Shift or Ctrl.
        virtual bool isModifier() const = 0;
        // Left or right Shift with no other modifier held.
        virtual bool isBareShift() const = 0;
        // The hardware keycode, and the app's timestamp of the key in ms or 0 if it sends none.
        virtual uint32_t code() const = 0;
        virtual uint32_t time() const = 0;

        // These describe the key the app will get, which replaceSym changes.
        virtual bool        hasModifier() const  = 0;
        virtual bool        isCursorMove() const = 0;
        virtual std::string name() const         = 0;

        // Hands the app this symbol, with the same modifiers, if the key goes on to it.
        virtual void replaceSym(uint32_t sym) = 0;
        // Keeps the key from the app.
        virtual void accept() = 0;
        // Does nothing, since a key not accepted goes on to the app anyway; marks the branches that
        // mean to let it through.
        void passToApp() {}
    };

} // namespace ngosen
