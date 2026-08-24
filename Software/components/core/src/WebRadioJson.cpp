/**
 * @file    WebRadioJson.cpp
 * @brief   WebRadioJson implementation.
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

namespace core {

namespace {

constexpr std::size_t kMaxUrlLength = 200U;
constexpr std::string_view kHttpPrefix = "http://";

[[nodiscard]] std::string_view extractJsonString(std::string_view json,
                                                  std::string_view key)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":";
    const std::size_t needlePos = json.find(needle);
    if (needlePos == std::string_view::npos) {
        return {};
    }
    std::size_t start = needlePos + needle.size();
    while (start < json.size()
           && (json[start] == ' ' || json[start] == '\t'
               || json[start] == '\r' || json[start] == '\n')) {
        ++start;
    }
    if (start >= json.size() || json[start] != '"') {
        return {};
    }
    const std::size_t valueStart = start + 1U;
    const std::size_t valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
        return {};
    }
    return json.substr(valueStart, valueEnd - valueStart);
}

[[nodiscard]] bool extractJsonBool(std::string_view json,
                                   std::string_view key, bool& out)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":";
    const std::size_t needlePos = json.find(needle);
    if (needlePos == std::string_view::npos) {
        return false;
    }
    std::size_t start = needlePos + needle.size();
    while (start < json.size()
           && (json[start] == ' ' || json[start] == '\t'
               || json[start] == '\r' || json[start] == '\n')) {
        ++start;
    }
    const std::string_view rest = json.substr(start);
    if (rest.starts_with("true")) {
        out = true;
        return true;
    }
    if (rest.starts_with("false")) {
        out = false;
        return true;
    }
    return false;
}

} // namespace

std::expected<WebRadioConfig, ParseError> parseWebRadioConfigJson(
    std::string_view json)
{
    bool enabled = false;
    if (!extractJsonBool(json, "enabled", enabled)) {
        return std::unexpected(ParseError::MissingField);
    }

    const std::string_view url = extractJsonString(json, "url");
    if (url.empty() || url.size() > kMaxUrlLength
        || url.substr(0, kHttpPrefix.size()) != kHttpPrefix) {
        return std::unexpected(ParseError::InvalidJson);
    }

    return WebRadioConfig{.enabled = enabled, .url = std::string(url)};
}

std::string serializeWebRadioConfigJson(const WebRadioConfig& config)
{
    return std::string(R"({"enabled":)") + (config.enabled ? "true" : "false")
        + R"(,"url":")" + config.url + "\"}";
}

std::string serializeWebRadioErrorJson(std::string_view reason)
{
    return std::string(R"({"status":"error","reason":")")
        + std::string(reason) + "\"}";
}

} // namespace core
