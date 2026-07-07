/**
 * @file    DspProgramBlob.hpp
 * @brief   Framed ADAU1701 program blob parse/serialise (pure core).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Blob layout (v1, little-endian):
 *   magic[4] = "DRAD", version u16, write_count u16, payload_crc32 u32,
 *   then for each write: address u16, length u16, data[length].
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include "core/DspProgram.hpp"
#include "core/DspProgramError.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace core {

/**
 * @brief    parseDspProgramBlob — validate and decode a flash/HTTP blob.
 *
 * @dname    parseDspProgramBlob
 * @param    blob  Raw bytes from the dsp partition or POST body.
 * @return   DspProgram on success, or DspProgramError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<DspProgram, DspProgramError>
parseDspProgramBlob(std::span<const std::uint8_t> blob);

/**
 * @brief    serializeDspProgramBlob — encode a program for flash storage.
 *
 * @dname    serializeDspProgramBlob
 * @param    program  Ordered download script.
 * @return   Framed blob bytes with CRC32.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::vector<std::uint8_t>
serializeDspProgramBlob(const DspProgram& program);

/**
 * @brief    dspProgramErrorToken — stable API/JSON error string.
 *
 * @dname    dspProgramErrorToken
 * @param    error  Parse or store failure.
 * @return   Short snake_case token.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] const char* dspProgramErrorToken(DspProgramError error) noexcept;

} // namespace core
