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

#include "core/JsonText.hpp"

#include <sstream>

namespace core {

namespace {

constexpr std::size_t kMaxUrlLength = 200U;
constexpr std::string_view kHttpPrefix = "http://";
constexpr std::string_view kHttpsPrefix = "https://";

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

    const std::string url = unescapeJsonString(extractJsonString(json, "url"));
    const bool isHttp = url.substr(0, kHttpPrefix.size()) == kHttpPrefix;
    const bool isHttps = url.substr(0, kHttpsPrefix.size()) == kHttpsPrefix;
    if (url.empty() || url.size() > kMaxUrlLength || !(isHttp || isHttps)) {
        return std::unexpected(ParseError::InvalidJson);
    }

    return WebRadioConfig{.enabled = enabled, .url = url};
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

namespace {

void appendJsonString(std::ostringstream& out, std::string_view value)
{
    out << '"';
    for (const char c : value) {
        if (c == '"' || c == '\\') {
            out << '\\';
        }
        out << c;
    }
    out << '"';
}

void appendFault(std::ostringstream& out, const WebRadioStatus& status)
{
    if (status.fault == WebRadioFault::None) {
        return;
    }
    out << R"(,"error":")" << webRadioFaultToken(status.fault) << '"';
    if (status.httpStatus != 0) {
        out << R"(,"http_status":)" << status.httpStatus;
    }
}

void appendFormat(std::ostringstream& out, const WebRadioStatus& status)
{
    if (!status.codec.empty()) {
        out << R"(,"codec":)";
        appendJsonString(out, status.codec);
    }
    if (status.sampleRateHz != 0U) {
        out << R"(,"sample_rate":)" << status.sampleRateHz
            << R"(,"channels":)" << static_cast<unsigned>(status.channels)
            << R"(,"bitrate_kbps":)" << status.bitrateKbps;
    }
}

} // namespace

std::string_view webRadioStateToken(WebRadioState state) noexcept
{
    switch (state) {
    case WebRadioState::Connecting: return "connecting";
    case WebRadioState::Buffering:  return "buffering";
    case WebRadioState::Playing:    return "playing";
    case WebRadioState::Error:      return "error";
    case WebRadioState::Idle:       break;
    }
    return "idle";
}

std::string_view webRadioFaultToken(WebRadioFault fault) noexcept
{
    static constexpr std::string_view kTokens[] = {
        "", "connect_failed", "http_status", "not_audio",
        "unsupported_format", "decode", "out_of_memory", "audio_output",
    };
    return kTokens[static_cast<std::size_t>(fault)];
}

std::string serializeWebRadioJson(const WebRadioConfig& config,
                                  const WebRadioStatus& status)
{
    std::ostringstream out;
    out << R"({"enabled":)" << (config.enabled ? "true" : "false")
        << R"(,"url":)";
    appendJsonString(out, config.url);
    out << R"(,"state":")" << webRadioStateToken(status.state) << '"';
    appendFault(out, status);
    appendFormat(out, status);
    out << R"(,"buffer_percent":)" << static_cast<unsigned>(status.bufferPercent)
        << R"(,"underruns":)" << status.underruns
        << R"(,"reconnects":)" << status.reconnects;
    if (status.ringBytes > 0U) {
        out << R"(,"ring_bytes":)" << status.ringBytes
            << R"(,"heap_min":)" << status.heapMinBytes;
    }
    if (status.title) {
        out << R"(,"title":)";
        appendJsonString(out, *status.title);
    }
    out << '}';
    return out.str();
}

} // namespace core
