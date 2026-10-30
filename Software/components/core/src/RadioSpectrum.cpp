/**
 * @file    RadioSpectrum.cpp
 * @brief   RF band scan JSON (GET/POST /api/tuner/spectrum).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/RadioSpectrum.hpp"

namespace core {

namespace {

constexpr std::string_view kBandKey = "\"band\"";

[[nodiscard]] std::string_view stateToken(SpectrumState state) noexcept
{
    switch (state) {
    case SpectrumState::Running:
        return "running";
    case SpectrumState::Done:
        return "done";
    case SpectrumState::Failed:
        return "failed";
    case SpectrumState::Idle:
        break;
    }
    return "idle";
}

void appendField(std::string& out, std::string_view key,
                 std::optional<int> value)
{
    if (value) {
        out += ",\"";
        out += key;
        out += "\":";
        out += std::to_string(*value);
    }
}

template <typename T>
[[nodiscard]] std::optional<int> asInt(const std::optional<T>& v)
{
    return v ? std::optional<int>(static_cast<int>(*v)) : std::nullopt;
}

void appendPoint(std::string& out, const SpectrumPoint& point)
{
    out += "{\"freq_khz\":" + std::to_string(point.freqKhz);
    if (point.dabChannel && *point.dabChannel < kDabBand3Channels.size()) {
        out += ",\"channel\":\"";
        out += kDabBand3Channels[*point.dabChannel];
        out += '"';
    }
    appendField(out, "rssi_dbuv", asInt(point.rssiDbuV));
    appendField(out, "snr_db", asInt(point.snrDb));
    appendField(out, "cnr_db", asInt(point.cnrDb));
    appendField(out, "fic_quality", asInt(point.ficQuality));
    out += '}';
}

} // namespace

std::uint8_t spectrumProgress(std::size_t done, std::size_t total) noexcept
{
    if (total == 0U || done >= total) {
        return 100U;
    }
    return static_cast<std::uint8_t>(done * 100U / total);
}

std::expected<TunerBand, ParseError>
parseSpectrumRequestJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::size_t key = json.find(kBandKey);
    if (key == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::size_t open = json.find('"', json.find(':', key));
    if (open == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::string_view value = json.substr(open + 1U);
    if (value.starts_with("fm\"")) {
        return TunerBand::Fm;
    }
    if (value.starts_with("dab\"")) {
        return TunerBand::Dab;
    }
    return std::unexpected(ParseError::InvalidJson);
}

std::string serializeSpectrumJson(const SpectrumSnapshot& snapshot)
{
    std::string out;
    out.reserve(64U + snapshot.points.size() * 56U);
    out += "{\"state\":\"";
    out += stateToken(snapshot.state);
    out += "\",\"band\":\"";
    out += snapshot.band == TunerBand::Fm ? "fm" : "dab";
    out += "\",\"progress\":" + std::to_string(snapshot.progress)
           + ",\"points\":[";
    for (std::size_t i = 0U; i < snapshot.points.size(); ++i) {
        if (i > 0U) {
            out += ',';
        }
        appendPoint(out, snapshot.points[i]);
    }
    out += "]}";
    return out;
}

} // namespace core
