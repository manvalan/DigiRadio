/**
 * @file    SignalQuality.cpp
 * @brief   SignalQuality implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "core/SignalQuality.hpp"

#include <algorithm>

namespace core {

namespace {

/** Map value from [lo, hi] onto 0–100, clamped. */
[[nodiscard]] int scale(int value, int lo, int hi)
{
    return std::clamp((value - lo) * 100 / (hi - lo), 0, 100);
}

[[nodiscard]] std::uint8_t fmScore(const TunerStatus& s)
{
    const int snr = scale(s.fmSnrDb.value_or(0), 3, 25);
    const int rssi = scale(s.fmRssiDbuV.value_or(0), 5, 45);
    const int score = (snr * 60 + rssi * 40) / 100;
    return static_cast<std::uint8_t>(s.locked ? score : std::min(score, 20));
}

[[nodiscard]] std::uint8_t dabScore(const TunerStatus& s)
{
    if (!s.locked) {
        return 0U;
    }
    const int fic = std::clamp<int>(s.dabFicQuality.value_or(0), 0, 100);
    if (!s.dabCnrDb) {
        return static_cast<std::uint8_t>(fic);
    }
    const int cnr = scale(*s.dabCnrDb, 5, 25);
    return static_cast<std::uint8_t>((fic * 70 + cnr * 30) / 100);
}

void appendInt(std::string& out, const char* key, std::optional<int> value)
{
    if (value) {
        out += std::string(",\"") + key + "\":" + std::to_string(*value);
    }
}

} // namespace

SignalQuality computeSignalQuality(const TunerStatus& status)
{
    if (status.band == TunerBand::Fm) {
        return SignalQuality{.band = status.band,
                             .locked = status.locked,
                             .levelDbuV = status.fmRssiDbuV,
                             .snrDb = status.fmSnrDb,
                             .quality = fmScore(status)};
    }
    return SignalQuality{.band = status.band,
                         .locked = status.locked,
                         .levelDbuV = status.dabRssiDbuV,
                         .snrDb = status.dabCnrDb,
                         .ficQuality = status.dabFicQuality,
                         .fibErrors = status.dabFibErrors,
                         .ficErrors = status.dabFicErrors,
                         .quality = dabScore(status)};
}

std::string_view signalGradeToken(std::uint8_t quality) noexcept
{
    if (quality >= 75U) {
        return "excellent";
    }
    if (quality >= 50U) {
        return "good";
    }
    if (quality >= 25U) {
        return "fair";
    }
    return quality > 0U ? "poor" : "none";
}

std::string serializeSignalQualityJson(const SignalQuality& signal)
{
    std::string out = std::string(R"({"band":")")
                      + (signal.band == TunerBand::Fm ? "fm" : "dab")
                      + R"(","locked":)" + (signal.locked ? "true" : "false");
    const auto asInt = [](auto v) -> std::optional<int> {
        return v ? std::optional<int>(static_cast<int>(*v)) : std::nullopt;
    };
    appendInt(out, "level_dbuv", asInt(signal.levelDbuV));
    appendInt(out, "snr_db", asInt(signal.snrDb));
    appendInt(out, "fic_quality", asInt(signal.ficQuality));
    appendInt(out, "fib_errors", asInt(signal.fibErrors));
    if (signal.ficErrors) {
        out += std::string(R"(,"fic_errors":)")
               + (*signal.ficErrors ? "true" : "false");
    }
    out += R"(,"quality":)" + std::to_string(signal.quality) + R"(,"grade":")"
           + std::string(signalGradeToken(signal.quality)) + "\"}";
    return out;
}

} // namespace core
