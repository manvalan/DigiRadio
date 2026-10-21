/**
 * @file    AfFollowJson.cpp
 * @brief   AfFollowJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */
#include "core/AfFollowJson.hpp"

#include <cstddef>

namespace core {

namespace {

constexpr std::string_view kEnabledKey = "\"enabled\":";

} // namespace

std::expected<bool, ParseError> parseAfFollowEnabledJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::size_t keyPos = json.find(kEnabledKey);
    if (keyPos == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::size_t start =
        json.find_first_not_of(" \t\r\n", keyPos + kEnabledKey.size());
    if (start == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::string_view rest = json.substr(start);
    if (rest.starts_with("true")) {
        return true;
    }
    if (rest.starts_with("false")) {
        return false;
    }
    return std::unexpected(ParseError::InvalidJson);
}

std::string serializeAfFollowJson(const AfFollowStats& stats)
{
    std::string out = std::string(R"({"enabled":)")
                      + (stats.enabled ? "true" : "false")
                      + R"(,"hunts":)" + std::to_string(stats.hunts)
                      + R"(,"switches":)" + std::to_string(stats.switches);
    if (stats.lastFrom && stats.lastTo) {
        out += R"(,"last_from_khz":)" + std::to_string(stats.lastFrom->value())
               + R"(,"last_to_khz":)" + std::to_string(stats.lastTo->value());
    }
    return out + "}";
}

} // namespace core
