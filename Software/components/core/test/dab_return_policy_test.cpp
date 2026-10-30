/**
 * @file    dab_return_policy_test.cpp
 * @brief   Host tests for DabReturnPolicy (FM fallback → DAB return).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/DabReturnPolicy.hpp"

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

constexpr std::uint32_t kMinute = 60000U;

core::TunerStatus fmWithPi(std::optional<std::uint16_t> pi)
{
    core::TunerStatus s{};
    s.band = core::TunerBand::Fm;
    s.fmProgramId = pi;
    return s;
}

core::DabReturnPolicy armedPolicy()
{
    core::DabReturnPolicy policy;
    policy.arm(core::DabReturnTarget{.freqIndex = 28U, .serviceId = 0x5201U,
                                     .componentId = 8U, .pi = 0x5201U},
               1000U);
    return policy;
}

void waitsOneIntervalAfterArming()
{
    const auto policy = armedPolicy();
    const auto fm = fmWithPi(0x5201U);
    expect(!policy.shouldTry(fm, 1000U + 10U * kMinute - 1U),
           "not before 10 minutes");
    expect(policy.shouldTry(fm, 1000U + 10U * kMinute), "at 10 minutes");
}

void attemptRestartsTheInterval()
{
    auto policy = armedPolicy();
    const auto fm = fmWithPi(std::nullopt);
    policy.recordAttempt(1000U + 10U * kMinute);
    expect(!policy.shouldTry(fm, 1000U + 15U * kMinute),
           "next try one interval after the failed one");
}

void zeroIntervalDisables()
{
    auto policy = armedPolicy();
    policy.setIntervalMinutes(0U);
    expect(!policy.shouldTry(fmWithPi(0x5201U), 1000U + 600U * kMinute),
           "0 = never");
}

void listenerMovingAwayIsStale()
{
    const auto policy = armedPolicy();
    core::TunerStatus dab{};
    dab.band = core::TunerBand::Dab;
    expect(policy.isStale(dab), "manual DAB tune ends the fallback");
    expect(policy.isStale(fmWithPi(0x1234U)), "other FM station ends it");
    expect(!policy.isStale(fmWithPi(0x5201U)), "AF with same PI keeps it");
    expect(!policy.isStale(fmWithPi(std::nullopt)), "PI not yet decoded");
}

void dabQualityGate()
{
    core::TunerStatus s{};
    s.band = core::TunerBand::Dab;
    s.locked = true;
    s.dabFicQuality = 59U;
    expect(!core::DabReturnPolicy::dabGoodEnough(s), "FIC 59 too weak");
    s.dabFicQuality = 60U;
    expect(core::DabReturnPolicy::dabGoodEnough(s), "FIC 60 good");
    s.locked = false;
    expect(!core::DabReturnPolicy::dabGoodEnough(s), "unlocked");
}

void wrapsAroundMillisecondCounter()
{
    core::DabReturnPolicy policy;
    policy.arm(core::DabReturnTarget{}, 0xFFFFFFF0U);
    expect(policy.shouldTry(fmWithPi(std::nullopt),
                            0xFFFFFFF0U + 10U * kMinute),
           "uint32 ms wrap");
}

} // namespace

int main()
{
    waitsOneIntervalAfterArming();
    attemptRestartsTheInterval();
    zeroIntervalDisables();
    listenerMovingAwayIsStale();
    dabQualityGate();
    wrapsAroundMillisecondCounter();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
