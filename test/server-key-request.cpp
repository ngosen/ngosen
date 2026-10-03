// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit test for parseKeyRequest(): the uinput server must refuse counts it cannot act on safely.
// A huge select allocates its key events up front; if that kills the server, the keyboard goes
// down with it.

#include "lotus-key-request.h"

#include <climits>
#include <iostream>
#include <string>

namespace {

    int  failures = 0;

    void expect(const std::string& name, KeyRequest got, KeyRequest::Kind kind, int count) {
        if (got.kind != kind || got.count != count) {
            std::cerr << "FAIL: " << name << '\n';
            ++failures;
        }
    }

} // namespace

int main() {
    constexpr ssize_t intSize = sizeof(int);
    using Kind                = KeyRequest::Kind;

    expect("one backspace", parseKeyRequest(intSize, 1), Kind::Backspace, 1);
    expect("max backspaces", parseKeyRequest(intSize, kMaxKeysPerRequest), Kind::Backspace, kMaxKeysPerRequest);
    expect("select 3", parseKeyRequest(intSize, -3), Kind::Select, 3);
    expect("max select", parseKeyRequest(intSize, -kMaxKeysPerRequest), Kind::Select, kMaxKeysPerRequest);

    expect("zero", parseKeyRequest(intSize, 0), Kind::Invalid, 0);
    expect("too many backspaces", parseKeyRequest(intSize, kMaxKeysPerRequest + 1), Kind::Invalid, 0);
    expect("too large select", parseKeyRequest(intSize, -kMaxKeysPerRequest - 1), Kind::Invalid, 0);
    expect("INT_MAX", parseKeyRequest(intSize, INT_MAX), Kind::Invalid, 0);
    expect("INT_MIN", parseKeyRequest(intSize, INT_MIN), Kind::Invalid, 0);
    expect("short message", parseKeyRequest(2, 1), Kind::Invalid, 0);
    expect("long message", parseKeyRequest(8, 1), Kind::Invalid, 0);

    return failures == 0 ? 0 : 1;
}
