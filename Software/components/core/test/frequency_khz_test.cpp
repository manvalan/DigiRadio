/**
 * @file    frequency_khz_test.cpp
 * @brief   Host tests for FrequencyKHz validation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/FrequencyKHz.hpp"
#include "core/ParseError.hpp"

#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] int runFrequencyAcceptTest()
{
    const auto parsed = core::FrequencyKHz::tryFromKhz(101500U);
    if (!parsed || parsed->value() != 101500U) {
        std::cerr << "expected 101500 kHz accepted\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runFrequencyRejectTest()
{
    const auto parsed = core::FrequencyKHz::tryFromKhz(50000U);
    if (parsed || parsed.error() != core::ParseError::MissingField) {
        std::cerr << "expected out-of-band frequency rejection\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runFrequencyAcceptTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runFrequencyRejectTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
