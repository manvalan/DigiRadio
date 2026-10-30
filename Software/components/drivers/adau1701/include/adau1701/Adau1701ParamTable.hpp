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
 * SigmaStudio export, "DigiRadioFinale-v2" revision, captured 2026-10-27).
 * Regenerate by re-running the extraction over that file if the SigmaStudio
 * project's cell list changes; entries here must stay in sync with the
 * ADDR_* constants used by
 * components/drivers/adau1701/src/Adau1701Driver.cpp's replayProgram().
 *
 * @author  Michele Bigi
 * @date    2026-08-25
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
 * @date     2026-08-25
 */
inline constexpr std::array<Adau1701ParamEntry, 227> kAdau1701ParamTable = {{
    {"BEEP1_ENABLE", 0},
    {"BEEP1_KICK", 1},
    {"BEEP1_BEEP_FREQ", 2},
    {"INPUTSELECTOT", 3},
    {"DCB1_POLE", 4},
    {"DCB2_POLE", 5},
    {"DCB4_POLE", 6},
    {"DCB3_POLE", 7},
    {"SSPLITTER1", 8},
    {"1XRTA1_TCONST", 9},
    {"1XRTA1_HOLD", 10},
    {"1XRTA1_DECAY", 11},
    {"1XRTA1", 2074},
    {"1XRTA2_TCONST", 12},
    {"1XRTA2_HOLD", 13},
    {"1XRTA2_DECAY", 14},
    {"1XRTA2", 2074},
    {"1XRTA3_TCONST", 15},
    {"1XRTA3_HOLD", 16},
    {"1XRTA3_DECAY", 17},
    {"1XRTA3", 2074},
    {"1XRTA4_TCONST", 18},
    {"1XRTA4_HOLD", 19},
    {"1XRTA4_DECAY", 20},
    {"1XRTA4", 2074},
    {"COMPRESSOR1_0", 21},
    {"COMPRESSOR1_1", 22},
    {"COMPRESSOR1_2", 23},
    {"COMPRESSOR1_3", 24},
    {"COMPRESSOR1_4", 25},
    {"COMPRESSOR1_5", 26},
    {"COMPRESSOR1_6", 27},
    {"COMPRESSOR1_7", 28},
    {"COMPRESSOR1_8", 29},
    {"COMPRESSOR1_9", 30},
    {"COMPRESSOR1_10", 31},
    {"COMPRESSOR1_11", 32},
    {"COMPRESSOR1_12", 33},
    {"COMPRESSOR1_13", 34},
    {"COMPRESSOR1_14", 35},
    {"COMPRESSOR1_15", 36},
    {"COMPRESSOR1_16", 37},
    {"COMPRESSOR1_17", 38},
    {"COMPRESSOR1_18", 39},
    {"COMPRESSOR1_19", 40},
    {"COMPRESSOR1_20", 41},
    {"COMPRESSOR1_21", 42},
    {"COMPRESSOR1_22", 43},
    {"COMPRESSOR1_23", 44},
    {"COMPRESSOR1_24", 45},
    {"COMPRESSOR1_25", 46},
    {"COMPRESSOR1_26", 47},
    {"COMPRESSOR1_27", 48},
    {"COMPRESSOR1_28", 49},
    {"COMPRESSOR1_29", 50},
    {"COMPRESSOR1_30", 51},
    {"COMPRESSOR1_31", 52},
    {"COMPRESSOR1_32", 53},
    {"COMPRESSOR1_33", 54},
    {"COMPRESSOR1_RMS", 55},
    {"COMPRESSOR1_POST_GAIN", 56},
    {"COMPRESSOR1_HOLD", 57},
    {"COMPRESSOR1_DECAY", 58},
    {"PARAMEQ1_ST0_B0", 59},
    {"PARAMEQ1_ST0_B1", 60},
    {"PARAMEQ1_ST0_B2", 61},
    {"PARAMEQ1_ST0_A0", 62},
    {"PARAMEQ1_ST0_A1", 63},
    {"PARAMEQ1_ST1_B0", 64},
    {"PARAMEQ1_ST1_B1", 65},
    {"PARAMEQ1_ST1_B2", 66},
    {"PARAMEQ1_ST1_A0", 67},
    {"PARAMEQ1_ST1_A1", 68},
    {"PARAMEQ1_ST2_B0", 69},
    {"PARAMEQ1_ST2_B1", 70},
    {"PARAMEQ1_ST2_B2", 71},
    {"PARAMEQ1_ST2_A0", 72},
    {"PARAMEQ1_ST2_A1", 73},
    {"PARAMEQ1_ST3_B0", 74},
    {"PARAMEQ1_ST3_B1", 75},
    {"PARAMEQ1_ST3_B2", 76},
    {"PARAMEQ1_ST3_A0", 77},
    {"PARAMEQ1_ST3_A1", 78},
    {"PARAMEQ1_ST4_B0", 79},
    {"PARAMEQ1_ST4_B1", 80},
    {"PARAMEQ1_ST4_B2", 81},
    {"PARAMEQ1_ST4_A0", 82},
    {"PARAMEQ1_ST4_A1", 83},
    {"PARAMEQ1_ST5_B0", 84},
    {"PARAMEQ1_ST5_B1", 85},
    {"PARAMEQ1_ST5_B2", 86},
    {"PARAMEQ1_ST5_A0", 87},
    {"PARAMEQ1_ST5_A1", 88},
    {"GAIN1", 89},
    {"GAIN2", 90},
    {"SPHAT1_B0LO0", 91},
    {"SPHAT1_B0LO1", 92},
    {"SPHAT1_B0LO2", 93},
    {"SPHAT1_B0LO3", 94},
    {"SPHAT1_B0LO4", 95},
    {"SPHAT1_B0LO5", 96},
    {"SPHAT1_B0LO6", 97},
    {"SPHAT1_B0LO7", 98},
    {"SPHAT1_B0LO8", 99},
    {"SPHAT1_B0LO9", 100},
    {"SPHAT1_GAINHI", 101},
    {"SPHAT1_GAINLO", 102},
    {"SPHAT1_GAININV", 103},
    {"SPHAT1_COMP_BASE0", 104},
    {"SPHAT1_COMP_BASE1", 105},
    {"SPHAT1_COMP_BASE2", 106},
    {"SPHAT1_COMP_BASE3", 107},
    {"SPHAT1_COMP_BASE4", 108},
    {"SPHAT1_COMP_BASE5", 109},
    {"SPHAT1_COMP_BASE6", 110},
    {"SPHAT1_COMP_BASE7", 111},
    {"SPHAT1_COMP_BASE8", 112},
    {"SPHAT1_COMP_BASE9", 113},
    {"SPHAT1_COMP_BASE10", 114},
    {"SPHAT1_COMP_BASE11", 115},
    {"SPHAT1_COMP_BASE12", 116},
    {"SPHAT1_COMP_BASE13", 117},
    {"SPHAT1_COMP_BASE14", 118},
    {"SPHAT1_COMP_BASE15", 119},
    {"SPHAT1_COMP_BASE16", 120},
    {"SPHAT1_COMP_BASE17", 121},
    {"SPHAT1_COMP_BASE18", 122},
    {"SPHAT1_COMP_BASE19", 123},
    {"SPHAT1_COMP_BASE20", 124},
    {"SPHAT1_COMP_BASE21", 125},
    {"SPHAT1_COMP_BASE22", 126},
    {"SPHAT1_COMP_BASE23", 127},
    {"SPHAT1_COMP_BASE24", 128},
    {"SPHAT1_COMP_BASE25", 129},
    {"SPHAT1_COMP_BASE26", 130},
    {"SPHAT1_COMP_BASE27", 131},
    {"SPHAT1_COMP_BASE28", 132},
    {"SPHAT1_COMP_BASE29", 133},
    {"SPHAT1_COMP_BASE30", 134},
    {"SPHAT1_COMP_BASE31", 135},
    {"SPHAT1_COMP_BASE32", 136},
    {"SPHAT1_COMP_BASE33", 137},
    {"SPHAT1_RMS", 138},
    {"SPHAT1_DECAY", 139},
    {"SPHAT1_HOLD", 140},
    {"SPHAT1_SPREAD1", 141},
    {"SPHAT1_SPREAD2", 142},
    {"MULTIPLE1", 143},
    {"MULTIPLE1_1", 144},
    {"BASSBOOST1_BASSFREQUENCY", 145},
    {"BASSBOOST1_B0", 146},
    {"BASSBOOST1_B1", 147},
    {"BASSBOOST1_B2", 148},
    {"BASSBOOST1_A1", 149},
    {"BASSBOOST1_A2", 150},
    {"BASSBOOST1_TABLE0", 151},
    {"BASSBOOST1_TABLE1", 152},
    {"BASSBOOST1_TABLE2", 153},
    {"BASSBOOST1_TABLE3", 154},
    {"BASSBOOST1_TABLE4", 155},
    {"BASSBOOST1_TABLE5", 156},
    {"BASSBOOST1_TABLE6", 157},
    {"BASSBOOST1_TABLE7", 158},
    {"BASSBOOST1_TABLE8", 159},
    {"BASSBOOST1_TABLE9", 160},
    {"BASSBOOST1_TABLE10", 161},
    {"BASSBOOST1_TABLE11", 162},
    {"BASSBOOST1_TABLE12", 163},
    {"BASSBOOST1_TABLE13", 164},
    {"BASSBOOST1_TABLE14", 165},
    {"BASSBOOST1_TABLE15", 166},
    {"BASSBOOST1_TABLE16", 167},
    {"BASSBOOST1_TABLE17", 168},
    {"BASSBOOST1_TABLE18", 169},
    {"BASSBOOST1_TABLE19", 170},
    {"BASSBOOST1_TABLE20", 171},
    {"BASSBOOST1_TABLE21", 172},
    {"BASSBOOST1_TABLE22", 173},
    {"BASSBOOST1_TABLE23", 174},
    {"BASSBOOST1_TABLE24", 175},
    {"BASSBOOST1_TABLE25", 176},
    {"BASSBOOST1_TABLE26", 177},
    {"BASSBOOST1_TABLE27", 178},
    {"BASSBOOST1_TABLE28", 179},
    {"BASSBOOST1_TABLE29", 180},
    {"BASSBOOST1_TABLE30", 181},
    {"BASSBOOST1_TABLE31", 182},
    {"BASSBOOST1_TABLE32", 183},
    {"BASSBOOST1_TIMECONSTANT", 184},
    {"LIMITER1_S1", 185},
    {"LIMITER1_INT1", 186},
    {"LIMITER1_S2", 187},
    {"LIMITER1_INT2", 188},
    {"LIMITER1_S3", 189},
    {"LIMITER1_INT3", 190},
    {"LIMITER1_S4", 191},
    {"LIMITER1_INT4", 192},
    {"LIMITER1_C1", 193},
    {"LIMITER1_C2", 194},
    {"LIMITER1_C3", 195},
    {"LIMITER1_THRESHOLD", 196},
    {"LIMITER1_RMS", 197},
    {"LIMITER1_DECAY", 198},
    {"LIMITER1_DECAYCOMPLEMENT", 199},
    {"LIMITER2_S1", 200},
    {"LIMITER2_INT1", 201},
    {"LIMITER2_S2", 202},
    {"LIMITER2_INT2", 203},
    {"LIMITER2_S3", 204},
    {"LIMITER2_INT3", 205},
    {"LIMITER2_S4", 206},
    {"LIMITER2_INT4", 207},
    {"LIMITER2_C1", 208},
    {"LIMITER2_C2", 209},
    {"LIMITER2_C3", 210},
    {"LIMITER2_THRESHOLD", 211},
    {"LIMITER2_RMS", 212},
    {"LIMITER2_DECAY", 213},
    {"LIMITER2_DECAYCOMPLEMENT", 214},
    {"1XRTA1_2_TCONST", 215},
    {"1XRTA1_2_HOLD", 216},
    {"1XRTA1_2_DECAY", 217},
    {"1XRTA1_2", 2074},
    {"1XRTA2_2_TCONST", 218},
    {"1XRTA2_2_HOLD", 219},
    {"1XRTA2_2_DECAY", 220},
    {"1XRTA2_2", 2074},
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
 * @date     2026-08-25
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
