/**
 * @file    dsp_param_json_test.cpp
 * @brief   Host tests for DspParamJson parse/serialise.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-18
 */

#include "core/DspParamJson.hpp"

#include <cstdlib>
#include <iostream>

namespace {

bool expectEqual(const std::string& actual, const std::string& expected)
{
    if (actual != expected) {
        std::cerr << "expected: " << expected << "\n  actual: " << actual
                  << "\n";
        return false;
    }
    return true;
}

[[nodiscard]] int runParseWriteTest()
{
    const auto parsed = core::parseDspParamWriteJson(
        R"({"name":"BEEP1_KICK","value":1.0})");
    if (!parsed) {
        std::cerr << "expected parse success\n";
        return EXIT_FAILURE;
    }
    if (parsed->name != "BEEP1_KICK" || parsed->value != 1.0F) {
        std::cerr << "unexpected parsed fields\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runParseNegativeValueTest()
{
    const auto parsed = core::parseDspParamWriteJson(
        R"({"name":"LIMITER1_THRESHOLD","value":-0.5})");
    if (!parsed || parsed->value != -0.5F) {
        std::cerr << "expected negative value to parse\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runParseMissingFieldTest()
{
    const auto parsed = core::parseDspParamWriteJson(R"({"name":"X"})");
    if (parsed) {
        std::cerr << "expected missing value to fail\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runParseInvalidJsonTest()
{
    const auto parsed = core::parseDspParamWriteJson("not json");
    if (parsed) {
        std::cerr << "expected invalid json to fail\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runSerialiseListTest()
{
    const std::vector<core::DspParamInfo> params = {
        {"BEEP1_ENABLE", 0},
        {"BEEP1_KICK", 1},
    };
    const std::string json = core::serializeDspParamListJson(params);
    if (!expectEqual(
            json,
            R"({"params":[{"name":"BEEP1_ENABLE","address":0},{"name":"BEEP1_KICK","address":1}]})")) {
        return EXIT_FAILURE;
    }

    const std::string emptyJson = core::serializeDspParamListJson({});
    if (!expectEqual(emptyJson, R"({"params":[]})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runSerialiseErrorTest()
{
    const std::string json = core::serializeDspParamErrorJson("not_found");
    if (!expectEqual(json, R"({"status":"error","reason":"not_found"})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runParseWriteTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runParseNegativeValueTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runParseMissingFieldTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runParseInvalidJsonTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runSerialiseListTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runSerialiseErrorTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
