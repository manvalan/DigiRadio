/**
 * @file    DspParamJson.cpp
 * @brief   DspParamJson implementation.
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
#include <sstream>

namespace core {

namespace {

[[nodiscard]] std::string_view extractJsonString(std::string_view json,
                                                  std::string_view key)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":\"";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t valueStart = start + needle.size();
    const std::size_t valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
        return {};
    }
    return json.substr(valueStart, valueEnd - valueStart);
}

[[nodiscard]] bool extractJsonFloat(std::string_view json,
                                    std::string_view key, float& out)
{
    const std::string needle = std::string("\"") + std::string(key) + "\":";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return false;
    }
    const std::size_t valueStart = start + needle.size();
    char* end = nullptr;
    out = std::strtof(json.data() + valueStart, &end);
    return end != json.data() + valueStart;
}

} // namespace

std::expected<DspParamWriteRequest, ParseError> parseDspParamWriteJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    const std::string_view name = extractJsonString(json, "name");
    if (name.empty()) {
        return std::unexpected(ParseError::MissingField);
    }

    float value = 0.0F;
    if (!extractJsonFloat(json, "value", value)) {
        return std::unexpected(ParseError::MissingField);
    }

    DspParamWriteRequest req;
    req.name.assign(name.begin(), name.end());
    req.value = value;
    return req;
}

std::string serializeDspParamListJson(const std::vector<DspParamInfo>& params)
{
    std::ostringstream out;
    out << "{\"params\":[";
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        out << "{\"name\":\"" << params[i].name << "\",\"address\":"
            << params[i].address << "}";
    }
    out << "]}";
    return out.str();
}

std::string serializeDspParamErrorJson(const char* reason)
{
    return std::string("{\"status\":\"error\",\"reason\":\"") + reason
           + "\"}";
}

} // namespace core
