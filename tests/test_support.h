// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>
#include <iostream>

// Shared reporting for the mod's test executable, matching cameraunlock-core's
// own runner: each file exports one RunXTests() returning its failure count and
// test_main.cpp sums them. The count lives in a Report the file owns rather
// than a file-scope global, so two test files cannot share one counter.
namespace SomaHT::testing {

class Report {
public:
    void Check(bool passed, const char* name) {
        if (passed) {
            std::cout << "  [PASS] " << name << "\n";
        } else {
            std::cout << "  [FAIL] " << name << "\n";
            ++failures_;
        }
    }

    int Failures() const { return failures_; }

private:
    int failures_ = 0;
};

inline bool NearEqual(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

}  // namespace SomaHT::testing
