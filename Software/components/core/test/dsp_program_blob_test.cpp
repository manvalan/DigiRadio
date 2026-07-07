/**
 * @file    dsp_program_blob_test.cpp
 * @brief   Host tests for ADAU1701 program blob parse/serialise.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/DspProgram.hpp"
#include "core/DspProgramBlob.hpp"
#include "core/DspProgramError.hpp"
#include "core/RegisterWrite.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

[[nodiscard]] core::DspProgram sampleProgram()
{
    return core::DspProgram(std::vector<core::RegisterWrite>{
        core::RegisterWrite(0x081CU, {0x00U, 0x1CU}),
        core::RegisterWrite(0x0400U, std::vector<std::uint8_t>(16U, 0xABU)),
    });
}

[[nodiscard]] int runRoundTripTest()
{
    const core::DspProgram original = sampleProgram();
    const std::vector<std::uint8_t> blob =
        core::serializeDspProgramBlob(original);
    const auto parsed = core::parseDspProgramBlob(blob);
    if (!parsed) {
        std::cerr << "round-trip parse failed\n";
        return EXIT_FAILURE;
    }
    if (parsed->writes().size() != original.writes().size()) {
        std::cerr << "write count mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runEmptyTest()
{
    const std::vector<std::uint8_t> empty;
    const auto parsed = core::parseDspProgramBlob(empty);
    if (parsed || parsed.error() != core::DspProgramError::Empty) {
        std::cerr << "expected empty error\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTruncatedTest()
{
    const std::vector<std::uint8_t> blob = {'D', 'R', 'A', 'D', 1, 0, 1, 0};
    const auto parsed = core::parseDspProgramBlob(blob);
    if (parsed || parsed.error() != core::DspProgramError::Truncated) {
        std::cerr << "expected truncated error\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runBadCrcTest()
{
    std::vector<std::uint8_t> blob = core::serializeDspProgramBlob(sampleProgram());
    blob.back() ^= 0xFFU;
    const auto parsed = core::parseDspProgramBlob(blob);
    if (parsed || parsed.error() != core::DspProgramError::BadCrc) {
        std::cerr << "expected bad_crc error\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runBadMagicTest()
{
    std::vector<std::uint8_t> blob = core::serializeDspProgramBlob(sampleProgram());
    blob[0] = 'X';
    const auto parsed = core::parseDspProgramBlob(blob);
    if (parsed || parsed.error() != core::DspProgramError::InvalidMagic) {
        std::cerr << "expected invalid_magic error\n";
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
    if (runEmptyTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTruncatedTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runBadCrcTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runBadMagicTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
