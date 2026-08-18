/**
 * @file    DspParamJson.hpp
 * @brief   JSON parse/serialise for generic ADAU1701 parameter access (pure core).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-18
 */
#pragma once

#include "core/ParseError.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace core {

/**
 * @brief    DspParamWriteRequest — parsed PUT /api/dsp/param body.
 *
 * @dname    DspParamWriteRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseDspParamWriteJson at the HTTP boundary.
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
struct DspParamWriteRequest {
    std::string name;  ///< Cell name, looked up against the driver's table.
    float value;       ///< SigmaStudio floating coefficient to write.
};

/**
 * @brief    DspParamInfo — one discoverable Parameter RAM cell.
 *
 * @dname    DspParamInfo
 * @return   n/a (type)
 * @pubstate Plain DTO; caller supplies the table (core stays hardware-agnostic).
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
struct DspParamInfo {
    std::string_view name; ///< Cell name as exported by SigmaStudio.
    unsigned address;      ///< Parameter RAM address for safeload writes.
};

/**
 * @brief    parseDspParamWriteJson — validate PUT /api/dsp/param body.
 *
 * @dname    parseDspParamWriteJson
 * @param    json  Untrusted request body from the HTTP handler.
 * @return   DspParamWriteRequest on success, or a ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
[[nodiscard]] std::expected<DspParamWriteRequest, ParseError>
parseDspParamWriteJson(std::string_view json);

/**
 * @brief    serializeDspParamListJson — serialise the discoverable cell list.
 *
 * @dname    serializeDspParamListJson
 * @param    params  Every named cell in the compiled DSP program.
 * @return   JSON object with a `params` array for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
[[nodiscard]] std::string serializeDspParamListJson(
    const std::vector<DspParamInfo>& params);

/**
 * @brief    serializeDspParamErrorJson — serialise a DSP param API error.
 *
 * @dname    serializeDspParamErrorJson
 * @param    reason  Short machine-readable cause (never a secret).
 * @return   JSON object string with status error and reason fields.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
[[nodiscard]] std::string serializeDspParamErrorJson(const char* reason);

} // namespace core
