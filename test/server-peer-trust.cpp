// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit test for isTrustedServerUid(): the addon talks only to a uinput server run by the proxy
// user, or to a private pair run by the user itself.

#include "lotus-utils.h"

#include <iostream>
#include <string>

namespace {

    int  failures = 0;

    void expect(const std::string& name, bool got, bool want) {
        if (got != want) {
            std::cerr << "FAIL: " << name << '\n';
            ++failures;
        }
    }

} // namespace

int main() {
    constexpr uid_t proxy   = 956;
    constexpr uid_t self    = 1000;
    constexpr uid_t other   = 1001;
    constexpr uid_t missing = static_cast<uid_t>(-1);

    expect("installed server", isTrustedServerUid(proxy, proxy, self, false), true);
    expect("installed server, private namespace", isTrustedServerUid(proxy, proxy, self, true), true);
    expect("private pair run by the user", isTrustedServerUid(self, proxy, self, true), true);

    expect("the user without a private namespace", isTrustedServerUid(self, proxy, self, false), false);
    expect("another user", isTrustedServerUid(other, proxy, self, false), false);
    expect("another user, private namespace", isTrustedServerUid(other, proxy, self, true), false);
    expect("root", isTrustedServerUid(0, proxy, self, false), false);
    expect("proxy user missing", isTrustedServerUid(missing, missing, self, false), false);

    return failures == 0 ? 0 : 1;
}
