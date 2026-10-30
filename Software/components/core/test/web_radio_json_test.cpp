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
#include <string>

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

[[nodiscard]] int runStatusTest()
{
    const core::WebRadioConfig config{.enabled = true,
                                      .url = "http://h/s.mp3"};
    core::WebRadioStatus idle;
    const std::string idleJson = core::serializeWebRadioJson(
        core::WebRadioConfig{.enabled = false, .url = "http://h/s.mp3"}, idle);
    if (idleJson
        != R"({"enabled":false,"url":"http://h/s.mp3","state":"idle",)"
           R"("buffer_percent":0,"underruns":0,"reconnects":0})") {
        std::cerr << "idle status json: " << idleJson << '\n';
        return EXIT_FAILURE;
    }
    core::WebRadioStatus playing;
    playing.state = core::WebRadioState::Playing;
    playing.codec = "mp3";
    playing.sampleRateHz = 44100U;
    playing.channels = 2U;
    playing.bitrateKbps = 128U;
    playing.bufferPercent = 55U;
    playing.title = std::string(R"(Say "Hi")");
    const std::string json = core::serializeWebRadioJson(config, playing);
    if (json
        != R"({"enabled":true,"url":"http://h/s.mp3","state":"playing",)"
           R"("codec":"mp3","sample_rate":44100,"channels":2,)"
           R"("bitrate_kbps":128,"buffer_percent":55,"underruns":0,)"
           R"("reconnects":0,"title":"Say \"Hi\""})") {
        std::cerr << "playing status json: " << json << '\n';
        return EXIT_FAILURE;
    }
    core::WebRadioStatus failed;
    failed.state = core::WebRadioState::Error;
    failed.fault = core::WebRadioFault::HttpStatus;
    failed.httpStatus = 404;
    const std::string err = core::serializeWebRadioJson(config, failed);
    if (err.find(R"("state":"error","error":"http_status","http_status":404)")
        == std::string::npos) {
        std::cerr << "error status json: " << err << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

[[nodiscard]] int runHeapFieldsTest()
{
    core::WebRadioConfig config{.enabled = true, .url = "http://a.it/s.mp3"};
    core::WebRadioStatus status;
    status.state = core::WebRadioState::Playing;
    const std::string idle = core::serializeWebRadioJson(config, status);
    status.ringBytes = 12288U;
    status.heapMinBytes = 15000U;
    const std::string live = core::serializeWebRadioJson(config, status);
    if (idle.find("ring_bytes") != std::string::npos
        || live.find(R"("ring_bytes":12288,"heap_min":15000)")
               == std::string::npos) {
        std::cerr << "ring_bytes/heap_min only while streaming: " << live
                  << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main()
{
    if (runHeapFieldsTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runStatusTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runRoundTripTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runValidationTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
