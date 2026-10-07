// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ngosen-xtest.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// Stands in for the X server's XTEST and records every key request the engine makes:
// count > 0 is that many BackSpace presses, count < 0 selects -count characters with Shift+Left.
class KeySenderProbe {
  public:
    KeySenderProbe() {
        setXTestSenderForTest([this](int count) {
            requests_.push_back(count);
            return true;
        });
    }
    ~KeySenderProbe() {
        setXTestSenderForTest({});
    }
    KeySenderProbe(const KeySenderProbe&)            = delete;
    KeySenderProbe& operator=(const KeySenderProbe&) = delete;

    int             requests() const {
        return static_cast<int>(requests_.size());
    }

    // Takes the oldest request not taken yet; requests arrive synchronously inside keyEvent.
    bool receive(int& count, const std::string& step = "receive key request", const std::string& expected = "a key request") {
        if (next_ >= requests_.size()) {
            std::cerr << "Step: " << step << "\nExpected: " << expected << "\nActual: none\n";
            return false;
        }
        count = requests_[next_++];
        return true;
    }

  private:
    std::vector<int> requests_;
    size_t           next_ = 0;
};
