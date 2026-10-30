/**
 * @file    radio_spectrum_test.cpp
 * @brief   Host tests for the RF spectrum model and JSON.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/RadioSpectrum.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool ok, std::string_view what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

void fmSweepHas206Points()
{
    expect(core::spectrumFmPointCount() == 206U, "87.5-108.0 / 100 kHz");
}

void dabPlanNamesMatchFrequencies()
{
    expect(core::kDabBand3Channels[0] == "5A"
               && core::kDabBand3FrequenciesKhz[0] == 174928U,
           "5A = 174.928 MHz");
    expect(core::kDabBand3Channels[37] == "13F"
               && core::kDabBand3FrequenciesKhz[37] == 239200U,
           "13F = 239.200 MHz");
}

void progressReaches100OnlyAtTheEnd()
{
    expect(core::spectrumProgress(0U, 206U) == 0U, "start 0%");
    expect(core::spectrumProgress(205U, 206U) == 99U, "last step 99%");
    expect(core::spectrumProgress(206U, 206U) == 100U, "done 100%");
}

void parsesBand()
{
    expect(core::parseSpectrumRequestJson(R"({"band":"dab"})")
               == core::TunerBand::Dab,
           "dab");
    expect(core::parseSpectrumRequestJson(R"({ "band" : "fm" })")
               == core::TunerBand::Fm,
           "fm with spaces");
    expect(core::parseSpectrumRequestJson(R"({"band":"am"})").error()
               == core::ParseError::InvalidJson,
           "unknown band rejected");
    expect(core::parseSpectrumRequestJson("{}").error()
               == core::ParseError::MissingField,
           "missing band");
}

void serializesFmAndDabPoints()
{
    core::SpectrumSnapshot snap;
    snap.state = core::SpectrumState::Running;
    snap.band = core::TunerBand::Dab;
    snap.progress = 50U;
    snap.points.push_back(core::SpectrumPoint{
        .freqKhz = 223936U, .rssiDbuV = 40, .dabChannel = 28U,
        .cnrDb = 18, .ficQuality = 97U});
    snap.points.push_back(core::SpectrumPoint{
        .freqKhz = 225648U, .rssiDbuV = 5, .dabChannel = 29U});
    const std::string json = core::serializeSpectrumJson(snap);
    expect(json
               == R"({"state":"running","band":"dab","progress":50,"points":[)"
                  R"({"freq_khz":223936,"channel":"12A","rssi_dbuv":40,)"
                  R"("cnr_db":18,"fic_quality":97},)"
                  R"({"freq_khz":225648,"channel":"12B","rssi_dbuv":5}]})",
           "DAB JSON");
    snap = core::SpectrumSnapshot{};
    snap.points.push_back(
        core::SpectrumPoint{.freqKhz = 87500U, .rssiDbuV = 12, .snrDb = 3});
    expect(core::serializeSpectrumJson(snap)
               == R"({"state":"idle","band":"fm","progress":0,"points":[)"
                  R"({"freq_khz":87500,"rssi_dbuv":12,"snr_db":3}]})",
           "FM JSON");
}

} // namespace

int main()
{
    fmSweepHas206Points();
    dabPlanNamesMatchFrequencies();
    progressReaches100OnlyAtTheEnd();
    parsesBand();
    serializesFmAndDabPoints();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
