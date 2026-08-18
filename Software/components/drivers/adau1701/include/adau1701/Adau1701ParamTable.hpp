/**
 * @file    Adau1701ParamTable.hpp
 * @brief   Name -> parameter RAM address table for the whole DSP program.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generated from Firmware/ADAU1701-Firmware/DigiRadio_IC_1_PARAM.h (the
 * SigmaStudio export). Regenerate by re-running the extraction over that
 * file if the SigmaStudio project's cell list changes; entries here must
 * stay in sync with the ADDR_* constants used by
 * components/drivers/adau1701/src/Adau1701Driver.cpp's replayProgram().
 *
 * @author  Michele Bigi
 * @date    2026-08-18
 */
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace adau1701 {

/** One addressable Parameter RAM cell from the compiled SigmaStudio graph. */
struct Adau1701ParamEntry {
    std::string_view name; ///< Cell name as exported by SigmaStudio.
    unsigned address;      ///< Parameter RAM address for safeload writes.
};

/**
 * @brief    kAdau1701ParamTable — every named Parameter RAM cell.
 *
 * @dname    kAdau1701ParamTable
 * @return   n/a (data)
 * @pubstate Read-only; see file header for regeneration instructions.
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
inline constexpr std::array<Adau1701ParamEntry, 74> kAdau1701ParamTable = {{
    {"BEEP1_ENABLE", 0},
    {"BEEP1_KICK", 1},
    {"BEEP1_BEEP_FREQ", 2},
    {"SI4674", 3},
    {"SI4674_1", 4},
    {"ESP32", 5},
    {"ESP32_1", 6},
    {"SINGLE1", 7},
    {"SSPLITTER1", 8},
    {"STMIXER1_ST0_VOLUME", 9},
    {"STMIXER1_ST1_VOLUME", 10},
    {"STMIXER1_ST2_VOLUME", 11},
    {"PARAMEQ1_ST0_B0", 12},
    {"PARAMEQ1_ST0_B1", 13},
    {"PARAMEQ1_ST0_B2", 14},
    {"PARAMEQ1_ST0_A0", 15},
    {"PARAMEQ1_ST0_A1", 16},
    {"PARAMEQ1_ST1_B0", 17},
    {"PARAMEQ1_ST1_B1", 18},
    {"PARAMEQ1_ST1_B2", 19},
    {"PARAMEQ1_ST1_A0", 20},
    {"PARAMEQ1_ST1_A1", 21},
    {"PARAMEQ1_ST2_B0", 22},
    {"PARAMEQ1_ST2_B1", 23},
    {"PARAMEQ1_ST2_B2", 24},
    {"PARAMEQ1_ST2_A0", 25},
    {"PARAMEQ1_ST2_A1", 26},
    {"PARAMEQ1_ST3_B0", 27},
    {"PARAMEQ1_ST3_B1", 28},
    {"PARAMEQ1_ST3_B2", 29},
    {"PARAMEQ1_ST3_A0", 30},
    {"PARAMEQ1_ST3_A1", 31},
    {"PARAMEQ1_ST4_B0", 32},
    {"PARAMEQ1_ST4_B1", 33},
    {"PARAMEQ1_ST4_B2", 34},
    {"PARAMEQ1_ST4_A0", 35},
    {"PARAMEQ1_ST4_A1", 36},
    {"PARAMEQ1_ST5_B0", 37},
    {"PARAMEQ1_ST5_B1", 38},
    {"PARAMEQ1_ST5_B2", 39},
    {"PARAMEQ1_ST5_A0", 40},
    {"PARAMEQ1_ST5_A1", 41},
    {"MULTIPLE1", 42},
    {"MULTIPLE1_1", 43},
    {"LIMITER1_S1", 44},
    {"LIMITER1_INT1", 45},
    {"LIMITER1_S2", 46},
    {"LIMITER1_INT2", 47},
    {"LIMITER1_S3", 48},
    {"LIMITER1_INT3", 49},
    {"LIMITER1_S4", 50},
    {"LIMITER1_INT4", 51},
    {"LIMITER1_C1", 52},
    {"LIMITER1_C2", 53},
    {"LIMITER1_C3", 54},
    {"LIMITER1_THRESHOLD", 55},
    {"LIMITER1_RMS", 56},
    {"LIMITER1_DECAY", 57},
    {"LIMITER1_DECAYCOMPLEMENT", 58},
    {"LIMITER2_S1", 59},
    {"LIMITER2_INT1", 60},
    {"LIMITER2_S2", 61},
    {"LIMITER2_INT2", 62},
    {"LIMITER2_S3", 63},
    {"LIMITER2_INT3", 64},
    {"LIMITER2_S4", 65},
    {"LIMITER2_INT4", 66},
    {"LIMITER2_C1", 67},
    {"LIMITER2_C2", 68},
    {"LIMITER2_C3", 69},
    {"LIMITER2_THRESHOLD", 70},
    {"LIMITER2_RMS", 71},
    {"LIMITER2_DECAY", 72},
    {"LIMITER2_DECAYCOMPLEMENT", 73},
}};

/**
 * @brief    findAdau1701ParamAddress — look up a cell's RAM address by name.
 *
 * @dname    findAdau1701ParamAddress
 * @param    name  Cell name, case-sensitive, matching kAdau1701ParamTable.
 * @return   The address if found, or std::nullopt.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
[[nodiscard]] inline std::optional<unsigned> findAdau1701ParamAddress(
    std::string_view name) noexcept
{
    for (const auto& entry : kAdau1701ParamTable) {
        if (entry.name == name) {
            return entry.address;
        }
    }
    return std::nullopt;
}

} // namespace adau1701
