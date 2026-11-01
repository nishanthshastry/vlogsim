// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Deliberately tiny, dependency-free test harness: each test file is its
// own executable, registered with CTest. Swap for Catch2/GoogleTest later
// if the suite outgrows this.

#include <iostream>

inline int g_failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::cerr << "CHECK failed: " #cond " at " << __FILE__ << ":" << __LINE__  \
                       << "\n";                                                        \
            g_failures++;                                                              \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                          \
    do {                                                                                        \
        auto _a = (a);                                                                          \
        auto _b = (b);                                                                          \
        if (!(_a == _b)) {                                                                       \
            std::cerr << "CHECK_EQ failed: " #a " == " #b " at " << __FILE__ << ":" << __LINE__ \
                       << " (" << _a << " vs " << _b << ")\n";                                  \
            g_failures++;                                                                        \
        }                                                                                        \
    } while (0)

#define TEST_MAIN_RETURN() return g_failures == 0 ? 0 : 1
