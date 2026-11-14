// SPDX-License-Identifier: GPL-3.0-or-later
//
// The settings window converts text through ConvertText, with the charsets it shows (#655).
#include "ngosen-dbus-object.h"

#include <fcitx-utils/dbus/objectvtable.h>

#include <iostream>
#include <string>

namespace {

    int  failures = 0;

    void check(const std::string& step, bool ok) {
        if (!ok) {
            std::cerr << "FAIL: " << step << '\n';
            ++failures;
        }
    }

    bool refused(fcitx::NgoSenDBusObject& object, const std::string& from, int32_t letterCase) {
        try {
            object.convertText("abc", from, "Unicode", letterCase, false);
        } catch (const fcitx::dbus::MethodCallError&) { return true; }
        return false;
    }

} // namespace

int main() {
    fcitx::NgoSenDBusObject object(nullptr);
    check("TCVN3 to Unicode", object.convertText("TiÕng ViÖt", "TCVN3 (ABC)", "Unicode", 0, false) == "Tiếng Việt");
    check("letter case and diacritics", object.convertText("Hµ Néi", "TCVN3 (ABC)", "Unicode", 1, true) == "HA NOI");
    check("unknown charset", refused(object, "EBCDIC", 0));
    check("unknown letter case", refused(object, "Unicode", 9));
    if (failures != 0)
        return 1;
    std::cout << "dbus_convert_text: all passed\n";
    return 0;
}
