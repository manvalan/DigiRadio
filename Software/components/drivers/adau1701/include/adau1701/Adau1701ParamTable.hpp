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
 * SigmaStudio export, "DigiRadioFinale" revision, captured 2026-08-25).
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
inline constexpr std::array<Adau1701ParamEntry, 230> kAdau1701ParamTable = {{
    {"BEEP1_ENABLE", 0},
    {"BEEP1_KICK", 1},
    {"BEEP1_BEEP_FREQ", 2},
    {"DC1", 3},
    {"DCB1_POLE", 4},
    {"DCB2_POLE", 5},
    {"DCB4_POLE", 6},
    {"DCB3_POLE", 7},
    {"SSPLITTER1", 8},
    {"COMPRESSOR1_0", 9},
    {"COMPRESSOR1_1", 10},
    {"COMPRESSOR1_2", 11},
    {"COMPRESSOR1_3", 12},
    {"COMPRESSOR1_4", 13},
    {"COMPRESSOR1_5", 14},
    {"COMPRESSOR1_6", 15},
    {"COMPRESSOR1_7", 16},
    {"COMPRESSOR1_8", 17},
    {"COMPRESSOR1_9", 18},
    {"COMPRESSOR1_10", 19},
    {"COMPRESSOR1_11", 20},
    {"COMPRESSOR1_12", 21},
    {"COMPRESSOR1_13", 22},
    {"COMPRESSOR1_14", 23},
    {"COMPRESSOR1_15", 24},
    {"COMPRESSOR1_16", 25},
    {"COMPRESSOR1_17", 26},
    {"COMPRESSOR1_18", 27},
    {"COMPRESSOR1_19", 28},
    {"COMPRESSOR1_20", 29},
    {"COMPRESSOR1_21", 30},
    {"COMPRESSOR1_22", 31},
    {"COMPRESSOR1_23", 32},
    {"COMPRESSOR1_24", 33},
    {"COMPRESSOR1_25", 34},
    {"COMPRESSOR1_26", 35},
    {"COMPRESSOR1_27", 36},
    {"COMPRESSOR1_28", 37},
    {"COMPRESSOR1_29", 38},
    {"COMPRESSOR1_30", 39},
    {"COMPRESSOR1_31", 40},
    {"COMPRESSOR1_32", 41},
    {"COMPRESSOR1_33", 42},
    {"COMPRESSOR1_RMS", 43},
    {"COMPRESSOR1_POST_GAIN", 44},
    {"COMPRESSOR1_HOLD", 45},
    {"COMPRESSOR1_DECAY", 46},
    {"1XRTA3_TCONST", 47},
    {"1XRTA3_HOLD", 48},
    {"1XRTA3_DECAY", 49},
    {"1XRTA3", 2074},
    {"1XRTA4_TCONST", 50},
    {"1XRTA4_HOLD", 51},
    {"1XRTA4_DECAY", 52},
    {"1XRTA4", 2074},
    {"1XRTA1_TCONST", 53},
    {"1XRTA1_HOLD", 54},
    {"1XRTA1_DECAY", 55},
    {"1XRTA1", 2074},
    {"1XRTA2_TCONST", 56},
    {"1XRTA2_HOLD", 57},
    {"1XRTA2_DECAY", 58},
    {"1XRTA2", 2074},
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
    {"SPHAT1_B0LO0", 89},
    {"SPHAT1_B0LO1", 90},
    {"SPHAT1_B0LO2", 91},
    {"SPHAT1_B0LO3", 92},
    {"SPHAT1_B0LO4", 93},
    {"SPHAT1_B0LO5", 94},
    {"SPHAT1_B0LO6", 95},
    {"SPHAT1_B0LO7", 96},
    {"SPHAT1_B0LO8", 97},
    {"SPHAT1_B0LO9", 98},
    {"SPHAT1_GAINHI", 99},
    {"SPHAT1_GAINLO", 100},
    {"SPHAT1_GAININV", 101},
    {"SPHAT1_COMP_BASE0", 102},
    {"SPHAT1_COMP_BASE1", 103},
    {"SPHAT1_COMP_BASE2", 104},
    {"SPHAT1_COMP_BASE3", 105},
    {"SPHAT1_COMP_BASE4", 106},
    {"SPHAT1_COMP_BASE5", 107},
    {"SPHAT1_COMP_BASE6", 108},
    {"SPHAT1_COMP_BASE7", 109},
    {"SPHAT1_COMP_BASE8", 110},
    {"SPHAT1_COMP_BASE9", 111},
    {"SPHAT1_COMP_BASE10", 112},
    {"SPHAT1_COMP_BASE11", 113},
    {"SPHAT1_COMP_BASE12", 114},
    {"SPHAT1_COMP_BASE13", 115},
    {"SPHAT1_COMP_BASE14", 116},
    {"SPHAT1_COMP_BASE15", 117},
    {"SPHAT1_COMP_BASE16", 118},
    {"SPHAT1_COMP_BASE17", 119},
    {"SPHAT1_COMP_BASE18", 120},
    {"SPHAT1_COMP_BASE19", 121},
    {"SPHAT1_COMP_BASE20", 122},
    {"SPHAT1_COMP_BASE21", 123},
    {"SPHAT1_COMP_BASE22", 124},
    {"SPHAT1_COMP_BASE23", 125},
    {"SPHAT1_COMP_BASE24", 126},
    {"SPHAT1_COMP_BASE25", 127},
    {"SPHAT1_COMP_BASE26", 128},
    {"SPHAT1_COMP_BASE27", 129},
    {"SPHAT1_COMP_BASE28", 130},
    {"SPHAT1_COMP_BASE29", 131},
    {"SPHAT1_COMP_BASE30", 132},
    {"SPHAT1_COMP_BASE31", 133},
    {"SPHAT1_COMP_BASE32", 134},
    {"SPHAT1_COMP_BASE33", 135},
    {"SPHAT1_RMS", 136},
    {"SPHAT1_DECAY", 137},
    {"SPHAT1_HOLD", 138},
    {"SPHAT1_SPREAD1", 139},
    {"SPHAT1_SPREAD2", 140},
    {"GENFILTER1_ST0_B0", 141},
    {"GENFILTER1_ST0_B1", 142},
    {"GENFILTER1_ST0_B2", 143},
    {"GENFILTER1_ST0_A1", 144},
    {"GENFILTER1_ST0_A2", 145},
    {"MULTIPLE1", 146},
    {"MULTIPLE1_1", 147},
    {"BASSBOOST1_BASSFREQUENCY", 148},
    {"BASSBOOST1_B0", 149},
    {"BASSBOOST1_B1", 150},
    {"BASSBOOST1_B2", 151},
    {"BASSBOOST1_A1", 152},
    {"BASSBOOST1_A2", 153},
    {"BASSBOOST1_TABLE0", 154},
    {"BASSBOOST1_TABLE1", 155},
    {"BASSBOOST1_TABLE2", 156},
    {"BASSBOOST1_TABLE3", 157},
    {"BASSBOOST1_TABLE4", 158},
    {"BASSBOOST1_TABLE5", 159},
    {"BASSBOOST1_TABLE6", 160},
    {"BASSBOOST1_TABLE7", 161},
    {"BASSBOOST1_TABLE8", 162},
    {"BASSBOOST1_TABLE9", 163},
    {"BASSBOOST1_TABLE10", 164},
    {"BASSBOOST1_TABLE11", 165},
    {"BASSBOOST1_TABLE12", 166},
    {"BASSBOOST1_TABLE13", 167},
    {"BASSBOOST1_TABLE14", 168},
    {"BASSBOOST1_TABLE15", 169},
    {"BASSBOOST1_TABLE16", 170},
    {"BASSBOOST1_TABLE17", 171},
    {"BASSBOOST1_TABLE18", 172},
    {"BASSBOOST1_TABLE19", 173},
    {"BASSBOOST1_TABLE20", 174},
    {"BASSBOOST1_TABLE21", 175},
    {"BASSBOOST1_TABLE22", 176},
    {"BASSBOOST1_TABLE23", 177},
    {"BASSBOOST1_TABLE24", 178},
    {"BASSBOOST1_TABLE25", 179},
    {"BASSBOOST1_TABLE26", 180},
    {"BASSBOOST1_TABLE27", 181},
    {"BASSBOOST1_TABLE28", 182},
    {"BASSBOOST1_TABLE29", 183},
    {"BASSBOOST1_TABLE30", 184},
    {"BASSBOOST1_TABLE31", 185},
    {"BASSBOOST1_TABLE32", 186},
    {"BASSBOOST1_TIMECONSTANT", 187},
    {"1XRTA2_2_TCONST", 188},
    {"1XRTA2_2_HOLD", 189},
    {"1XRTA2_2_DECAY", 190},
    {"1XRTA2_2", 2074},
    {"1XRTA1_2_TCONST", 191},
    {"1XRTA1_2_HOLD", 192},
    {"1XRTA1_2_DECAY", 193},
    {"1XRTA1_2", 2074},
    {"LIMITER2_S1", 194},
    {"LIMITER2_INT1", 195},
    {"LIMITER2_S2", 196},
    {"LIMITER2_INT2", 197},
    {"LIMITER2_S3", 198},
    {"LIMITER2_INT3", 199},
    {"LIMITER2_S4", 200},
    {"LIMITER2_INT4", 201},
    {"LIMITER2_C1", 202},
    {"LIMITER2_C2", 203},
    {"LIMITER2_C3", 204},
    {"LIMITER2_THRESHOLD", 205},
    {"LIMITER2_RMS", 206},
    {"LIMITER2_DECAY", 207},
    {"LIMITER2_DECAYCOMPLEMENT", 208},
    {"LIMITER1_S1", 209},
    {"LIMITER1_INT1", 210},
    {"LIMITER1_S2", 211},
    {"LIMITER1_INT2", 212},
    {"LIMITER1_S3", 213},
    {"LIMITER1_INT3", 214},
    {"LIMITER1_S4", 215},
    {"LIMITER1_INT4", 216},
    {"LIMITER1_C1", 217},
    {"LIMITER1_C2", 218},
    {"LIMITER1_C3", 219},
    {"LIMITER1_THRESHOLD", 220},
    {"LIMITER1_RMS", 221},
    {"LIMITER1_DECAY", 222},
    {"LIMITER1_DECAYCOMPLEMENT", 223},
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
