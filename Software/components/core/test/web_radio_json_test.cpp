/**
 * @file    web_radio_json_test.cpp
 * @brief   Host tests for WebRadioConfig JSON round-trip.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-08
 */

#include "core/WebRadioJson.hpp"

#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] int runRoundTripTest()
{
    const core::WebRadioConfig config{
        .enabled = true, .url = "http://edge.radiomontecarlo.net/RMC.mp3"};
    const std::string json = core::serializeWebRadioConfigJson(config);
    const auto parsed = core::parseWebRadioConfigJson(json);
    if (!parsed || parsed->enabled != true || parsed->url != config.url) {
        std::cerr << "round-trip mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runValidationTest()
{
    const auto missingUrl =
        core::parseWebRadioConfigJson(R"({"enabled":false})");
    if (missingUrl) {
        std::cerr << "missing url should fail\n";
        return EXIT_FAILURE;
    }
    const auto badScheme = core::parseWebRadioConfigJson(
        R"({"enabled":true,"url":"ftp://example.com/x.mp3"})");
    if (badScheme) {
        std::cerr << "non-http(s) scheme should fail\n";
        return EXIT_FAILURE;
    }
    // Most public internet radio streams are HTTPS-only; rejecting the
    // scheme outright (as this parser used to) made streaming unusable for
    // essentially any real station.
    const auto httpsOk = core::parseWebRadioConfigJson(
        R"({"enabled":true,"url":"https://example.com/x.mp3"})");
    if (!httpsOk || httpsOk->url != "https://example.com/x.mp3") {
        std::cerr << "https url should parse\n";
        return EXIT_FAILURE;
    }
    const auto malformed = core::parseWebRadioConfigJson("not json");
    if (malformed) {
        std::cerr << "malformed body should fail\n";
        return EXIT_FAILURE;
    }
    const auto ok = core::parseWebRadioConfigJson(
        R"({"enabled":false,"url":"http://example.com/x.mp3"})");
    if (!ok || ok->enabled != false) {
        std::cerr << "valid disabled config should parse\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runRoundTripTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runValidationTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
