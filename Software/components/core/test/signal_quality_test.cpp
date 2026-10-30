/**
 * @file    signal_quality_test.cpp
 * @brief   Host tests for computeSignalQuality and its JSON.
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

#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

[[nodiscard]] core::TunerStatus fm(std::int8_t rssi, std::int8_t snr,
                                   bool locked)
{
    core::TunerStatus s{};
    s.band = core::TunerBand::Fm;
    s.locked = locked;
    s.fmRssiDbuV = rssi;
    s.fmSnrDb = snr;
    return s;
}

[[nodiscard]] bool fmTest()
{
    // 40 dBuV / 26 dB (the real radio on 98.4 MHz): SNR 100, RSSI 87.
    const auto strong = core::computeSignalQuality(fm(40, 26, true));
    if (strong.quality != 94U || core::signalGradeToken(strong.quality) != "excellent") {
        return fail("strong FM must score 94 / excellent");
    }
    const auto weak = core::computeSignalQuality(fm(10, 5, true));
    if (weak.quality != 10U || core::signalGradeToken(weak.quality) != "poor") {
        return fail("weak FM must score 10 / poor");
    }
    const auto unlocked = core::computeSignalQuality(fm(40, 26, false));
    return unlocked.quality == 20U || fail("no VALID caps FM at 20");
}

[[nodiscard]] bool dabTest()
{
    core::TunerStatus s{};
    s.band = core::TunerBand::Dab;
    s.locked = true;
    s.dabFicQuality = 80U;
    s.dabCnrDb = 15;
    s.dabRssiDbuV = 30;
    const auto q = core::computeSignalQuality(s);
    if (q.quality != 71U || q.levelDbuV != 30) {
        return fail("DAB 80 FIC + 15 dB CNR must score 71");
    }
    const std::string json = core::serializeSignalQualityJson(q);
    if (json != R"({"band":"dab","locked":true,"level_dbuv":30,"snr_db":15,)"
                R"("fic_quality":80,"quality":71,"grade":"good"})") {
        std::cerr << json << '\n';
        return fail("DAB signal JSON mismatch");
    }
    s.locked = false;
    return core::computeSignalQuality(s).quality == 0U
        || fail("DAB without lock scores 0");
}

[[nodiscard]] bool dabErrorCountersTest()
{
    core::TunerStatus s{};
    s.band = core::TunerBand::Dab;
    s.locked = true;
    s.dabFicQuality = 100U;
    s.dabFibErrors = 42U;
    s.dabFicErrors = false;
    const std::string json =
        core::serializeSignalQualityJson(core::computeSignalQuality(s));
    return json.find(R"("fib_errors":42,"fic_errors":false)")
               != std::string::npos
        || fail("DAB JSON carries fib_errors and fic_errors");
}

} // namespace

int main()
{
    return fmTest() && dabTest() && dabErrorCountersTest() ? EXIT_SUCCESS
                                                         : EXIT_FAILURE;
}
