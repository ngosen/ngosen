// SPDX-License-Identifier: GPL-3.0-or-later
#include "ngosen-surrounding-count.h"

#include <iostream>
#include <string>

namespace {
    int failures = 0;

    using Match = ngosen::SurroundingDeletion::Match;

    void expect(const std::string& name, const ngosen::SurroundingDeletion& got, Match match, unsigned int chars, const std::string& retype = {}) {
        if (got.match == match && got.chars == chars && got.retype == retype)
            return;
        std::cerr << "FAIL: " << name << ": chars=" << got.chars << " retype='" << got.retype << "' match=" << static_cast<int>(got.match) << '\n';
        ++failures;
    }
} // namespace

int main() {
    expect("a fresh copy asks for the deleted characters", ngosen::countSurroundingDeletion("abc tie", "ti", "e", {}), Match::Fresh, 1);
    expect("a fresh copy with a multibyte part", ngosen::countSurroundingDeletion("tiê", "ti", "ê", {"tie"}), Match::Fresh, 1);
    // Qt on GNOME keeps reporting "tie" while the app holds "tiê": two copy characters are ê's bytes.
    expect("a copy showing an earlier state asks for the same bytes", ngosen::countSurroundingDeletion("tie", "ti", "ê", {"tie"}), Match::OldTail, 2);
    // On s the word "tiê" becomes "tíê" with the prefix "t"; the copy still shows "tie" after text.
    expect("an earlier state with a shorter prefix", ngosen::countSurroundingDeletion("nam tie", "t", "íê", {"tie", "tiê"}), Match::OldTail, 4);
    expect("an earlier state matches through a longer list", ngosen::countSurroundingDeletion("nguo", "ng", "ươ", {"nguo", "nguơ"}), Match::OldTail, 4);
    // The copy's ê is two bytes but the app holds a one-byte e: delete "ie" and type the i again.
    expect("no boundary at the deleted part deletes further back and retypes", ngosen::countSurroundingDeletion("tiê", "ti", "e", {"tiê"}), Match::OldTail, 1, "i");
    expect("a copy that says nothing about the word", ngosen::countSurroundingDeletion("xyz", "ti", "e", {"ta"}), Match::None, 1);
    expect("no old tails and a stale copy", ngosen::countSurroundingDeletion("tie", "ti", "ê", {}), Match::None, 1);
    expect("the word would have to be crossed", ngosen::countSurroundingDeletion("ê", "", "e", {"ê"}), Match::None, 1);
    if (failures == 0)
        std::cout << "all surrounding count checks passed\n";
    return failures == 0 ? 0 : 1;
}
