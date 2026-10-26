/**
 * @file    web_radio_budget_test.cpp
 * @brief   Host tests for the web radio heap budget.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/WebRadioBudget.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(bool ok, std::string_view what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

constexpr std::size_t kKb = 1024U;

} // namespace

int main()
{
    // 0.12.3 at stream start: ~65 KB free, 31.7 KB largest block.
    expect(core::chooseWebRadioRing(65U * kKb, 31U * kKb, false) == 24U * kKb,
           "65 KB free, http: 24 KB ring leaves 41 KB");
    expect(core::chooseWebRadioRing(65U * kKb, 31U * kKb, true) == std::nullopt,
           "65 KB free, https: refused (needs 56 KB + ring)");
    expect(core::chooseWebRadioRing(73U * kKb, 31U * kKb, true) == 16U * kKb,
           "73 KB free, https: 16 KB ring");
    expect(core::chooseWebRadioRing(80U * kKb, 14U * kKb, false) == 12U * kKb,
           "fragmented heap: smallest ring that fits one block");
    expect(core::chooseWebRadioRing(50U * kKb, 31U * kKb, false) == std::nullopt,
           "50 KB free: refused rather than starving Wi-Fi");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
