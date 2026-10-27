/**
 * @file    af_follower_test.cpp
 * @brief   Host tests for AlternateFrequencyFollower (RDS AF policy).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "core/AfFollowJson.hpp"
#include "core/AlternateFrequencyFollower.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr std::uint16_t kPi = 0x5201U;

[[nodiscard]] core::FrequencyKHz khz(std::uint32_t value)
{
    return *core::FrequencyKHz::tryFromKhz(value);
}

[[nodiscard]] core::TunerStatus fmStatus(std::uint32_t freq,
                                         std::int8_t rssi,
                                         bool locked,
                                         std::vector<std::uint32_t> afs = {})
{
    core::TunerStatus status{};
    status.booted = true;
    status.band = core::TunerBand::Fm;
    status.locked = locked;
    status.fmFrequency = khz(freq);
    status.fmRssiDbuV = rssi;
    status.fmProgramId = kPi;
    for (const std::uint32_t af : afs) {
        status.fmAlternateFrequencies.push_back(khz(af));
    }
    return status;
}

[[nodiscard]] core::AfSample sample(std::uint32_t freq,
                                    std::int8_t rssi,
                                    bool locked = true,
                                    std::optional<std::uint16_t> pi = kPi)
{
    return core::AfSample{khz(freq), rssi, locked, pi};
}

[[nodiscard]] bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

/* A strong signal never hunts; a weak one hunts only after three weak
 * snapshots in a row and the settle window, and not again inside the
 * cooldown. */
[[nodiscard]] bool huntTimingTest()
{
    core::AlternateFrequencyFollower follower;
    follower.observe(fmStatus(98400U, 30, true, {87600U, 101200U}), 0U);
    if (follower.shouldHunt(60000U)) {
        return fail("strong signal must not hunt");
    }
    follower.observe(fmStatus(98400U, 8, true), 1000U);
    follower.observe(fmStatus(98400U, 8, true), 2000U);
    follower.observe(fmStatus(98400U, 8, true), 3000U);
    if (follower.shouldHunt(5000U)) {
        return fail("must wait the settle window after a new station");
    }
    if (!follower.shouldHunt(9000U)) {
        return fail("sustained weak signal after settle must hunt");
    }
    follower.finishHunt(khz(98400U), 9000U);
    for (std::uint32_t t = 10000U; t <= 12000U; t += 1000U) {
        follower.observe(fmStatus(98400U, 8, true), t);
    }
    if (follower.shouldHunt(20000U)) {
        return fail("must respect the cooldown");
    }
    return follower.shouldHunt(29000U) || fail("hunt again after cooldown");
}

/* One or two bad snapshots (VALID flicker, RSSI dip) must not hunt. */
[[nodiscard]] bool flickerTest()
{
    core::AlternateFrequencyFollower follower;
    follower.observe(fmStatus(98400U, 36, true, {87600U}), 0U);
    follower.observe(fmStatus(98400U, 36, false), 10000U);
    follower.observe(fmStatus(98400U, 9, true), 13000U);
    if (follower.shouldHunt(14000U)) {
        return fail("two weak snapshots must not hunt");
    }
    follower.observe(fmStatus(98400U, 36, true), 16000U);
    follower.observe(fmStatus(98400U, 9, true), 19000U);
    follower.observe(fmStatus(98400U, 9, true), 22000U);
    return !follower.shouldHunt(23000U) || fail("a good snapshot resets the streak");
}

/* Without any AF there is nothing to probe. */
[[nodiscard]] bool noAfTest()
{
    core::AlternateFrequencyFollower follower;
    follower.observe(fmStatus(98400U, 2, false), 0U);
    return !follower.shouldHunt(60000U) || fail("no AF list: never hunt");
}

/* Candidates exclude the current frequency and are capped. */
[[nodiscard]] bool candidatesTest()
{
    core::AfFollowConfig config;
    config.maxCandidates = 2U;
    core::AlternateFrequencyFollower follower(config);
    follower.observe(
        fmStatus(98400U, 8, true, {98400U, 87600U, 101200U, 105300U}), 0U);
    const auto list = follower.candidates();
    if (list.size() != 2U) {
        return fail("expected 2 capped candidates");
    }
    return (list[0].value() == 87600U && list[1].value() == 101200U)
        || fail("candidates must follow RDS order without current");
}

/* Switch only for a clear gain; an unusable current takes any usable AF. */
[[nodiscard]] bool chooseTest()
{
    core::AlternateFrequencyFollower follower;
    follower.observe(fmStatus(98400U, 10, true, {87600U, 101200U}), 0U);
    const std::vector<core::AfSample> small{sample(87600U, 14)};
    if (follower.choose(sample(98400U, 10), small).value() != 98400U) {
        return fail("+4 dB is not enough to switch");
    }
    const std::vector<core::AfSample> big{sample(87600U, 14),
                                          sample(101200U, 25)};
    if (follower.choose(sample(98400U, 10), big).value() != 101200U) {
        return fail("must pick the strongest AF with >= 6 dB gain");
    }
    const std::vector<core::AfSample> weak{sample(87600U, 7)};
    if (follower.choose(sample(98400U, 0, false), weak).value() != 87600U) {
        return fail("unusable current must take any usable AF");
    }
    const std::vector<core::AfSample> dead{sample(87600U, 30, false)};
    return follower.choose(sample(98400U, 0, false), dead).value() == 98400U
        || fail("an unlocked AF is never chosen");
}

/* An AF carrying a different PI is another programme: never switch. */
[[nodiscard]] bool piMismatchTest()
{
    core::AlternateFrequencyFollower follower;
    follower.observe(fmStatus(98400U, 4, true, {87600U}), 0U);
    const std::vector<core::AfSample> other{
        sample(87600U, 40, true, std::uint16_t{0x5A01U})};
    if (follower.choose(sample(98400U, 4), other).value() != 98400U) {
        return fail("different PI must be rejected");
    }
    const std::vector<core::AfSample> unknown{
        sample(87600U, 40, true, std::nullopt)};
    return follower.choose(sample(98400U, 4), unknown).value() == 87600U
        || fail("AF without decoded PI yet is accepted");
}

/* After following to an AF the station (and its old frequency) is kept;
 * tuning outside the set starts a new station. */
[[nodiscard]] bool stationTrackingTest()
{
    core::AlternateFrequencyFollower follower;
    follower.observe(fmStatus(98400U, 8, true, {87600U}), 0U);
    follower.finishHunt(khz(87600U), 9000U);
    follower.observe(fmStatus(87600U, 30, true), 9500U);
    const auto back = follower.candidates();
    if (back.size() != 1U || back[0].value() != 98400U) {
        return fail("old frequency must stay a candidate after a switch");
    }
    follower.observe(fmStatus(105300U, 30, true), 10000U);
    return follower.candidates().empty()
        || fail("tuning outside the AF set must start a new station");
}

/* POST settings parsing and the GET response shape. */
[[nodiscard]] bool jsonTest()
{
    const auto off = core::parseAfFollowSettingsJson(R"({"enabled": false})");
    const auto both = core::parseAfFollowSettingsJson(
        R"({"enabled":true,"dab_return_min":15})");
    const auto minutesOnly =
        core::parseAfFollowSettingsJson(R"({"dab_return_min":0})");
    if (!off || off->enabled != false || off->dabReturnMinutes
        || !both || both->enabled != true || both->dabReturnMinutes != 15U
        || !minutesOnly || minutesOnly->enabled
        || minutesOnly->dabReturnMinutes != 0U) {
        return fail("settings must parse");
    }
    if (core::parseAfFollowSettingsJson("{}").error()
            != core::ParseError::MissingField
        || core::parseAfFollowSettingsJson(R"({"enabled":1})").error()
               != core::ParseError::InvalidJson
        || core::parseAfFollowSettingsJson(R"({"dab_return_min":121})")
                   .error()
               != core::ParseError::InvalidJson) {
        return fail("missing / bad settings must be rejected");
    }
    core::AfFollowStats stats;
    if (core::serializeAfFollowJson(stats)
        != R"({"enabled":false,"hunts":0,"switches":0,)"
           R"("dab_hunts":0,"dab_switches":0,"dab_return_min":10,)"
           R"("on_fm_fallback":false,"dab_returns":0})") {
        return fail("default stats JSON mismatch");
    }
    stats.enabled = true;
    stats.switches = 2U;
    stats.lastFrom = khz(98400U);
    stats.lastTo = khz(87600U);
    return core::serializeAfFollowJson(stats)
               == R"({"enabled":true,"hunts":0,"switches":2,"last_from_khz":98400,)"
                  R"("last_to_khz":87600,"dab_hunts":0,"dab_switches":0,)"
                  R"("dab_return_min":10,"on_fm_fallback":false,"dab_returns":0})"
        || fail("switch stats JSON mismatch");
}

} // namespace

int main()
{
    const bool ok = huntTimingTest() && flickerTest() && noAfTest() && candidatesTest()
                 && chooseTest() && piMismatchTest() && stationTrackingTest()
                 && jsonTest();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
