/**
 * @file    TunerJson.cpp
 * @brief   TunerJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/TunerJson.hpp"

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

[[nodiscard]] bool extractJsonUint(std::string_view json,
                                   std::string_view key,
                                   unsigned long& out)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return false;
    }
    const std::size_t valueStart = start + needle.size();
    char* end = nullptr;
    out = std::strtoul(json.data() + valueStart, &end, 10);
    return end != json.data() + valueStart;
}

[[nodiscard]] const char* bandToken(TunerBand band) noexcept
{
    return band == TunerBand::Fm ? "fm" : "dab";
}

void appendJsonString(std::ostringstream& out, std::string_view value)
{
    out << '"';
    for (char c : value) {
        if (c == '"' || c == '\\') {
            out << '\\';
        }
        out << c;
    }
    out << '"';
}

void appendOptionalLabel(std::ostringstream& out,
                         std::string_view key,
                         const std::optional<BroadcastLabel>& label)
{
    if (!label) {
        return;
    }
    out << ",\"" << key << "\":";
    appendJsonString(out, label->value());
}

} // namespace

std::string serializeTunerStatusJson(const TunerStatus& status)
{
    std::ostringstream out;
    out << "{\"booted\":" << (status.booted ? "true" : "false")
        << ",\"band\":\"" << bandToken(status.band) << "\""
        << ",\"locked\":" << (status.locked ? "true" : "false")
        << ",\"volume\":" << static_cast<unsigned>(status.volume);

    if (status.band == TunerBand::Dab) {
        out << ",\"dab\":{";
        if (status.dabFreqIndex) {
            out << "\"freq_index\":" << static_cast<unsigned>(*status.dabFreqIndex);
        }
        if (status.dabFicQuality) {
            if (status.dabFreqIndex) {
                out << ',';
            }
            out << "\"fic_quality\":" << static_cast<unsigned>(*status.dabFicQuality);
        }
        if (status.dabCnrDb) {
            out << ",\"cnr_db\":" << static_cast<int>(*status.dabCnrDb);
        }
        if (status.dabPlayingServiceId) {
            out << ",\"playing_service_id\":"
                << *status.dabPlayingServiceId;
        }
        if (status.dabPlayingComponentId) {
            out << ",\"playing_component_id\":"
                << *status.dabPlayingComponentId;
        }
        appendOptionalLabel(out, "dynamic_label", status.dabDynamicLabel);
        out << "},\"fm\":null";
    } else {
        out << ",\"fm\":{";
        if (status.fmFrequency) {
            out << "\"frequency_khz\":" << status.fmFrequency->value();
        }
        if (status.fmRssiDbuV) {
            if (status.fmFrequency) {
                out << ',';
            }
            out << "\"rssi_dbuv\":" << static_cast<int>(*status.fmRssiDbuV);
        }
        if (status.fmSnrDb) {
            out << ",\"snr_db\":" << static_cast<int>(*status.fmSnrDb);
        }
        if (status.fmFreqOffBppm) {
            out << ",\"freqoff_ppm\":"
                << (static_cast<int>(*status.fmFreqOffBppm) * 2);
        }
        if (status.fmStereo) {
            out << ",\"stereo\":" << (*status.fmStereo ? "true" : "false");
        }
        appendOptionalLabel(out, "station_name", status.fmStationName);
        appendOptionalLabel(out, "radiotext", status.fmRadiotext);
        out << "},\"dab\":null";
    }
    out << '}';
    return out.str();
}

std::string serializeTunerServicesJson(
    const std::vector<TunerServiceEntry>& services)
{
    std::ostringstream out;
    out << "{\"services\":[";
    for (std::size_t i = 0; i < services.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        const auto& s = services[i];
        std::string_view labelText(s.label.data(), s.label.size());
        const std::size_t nul = labelText.find('\0');
        if (nul != std::string_view::npos) {
            labelText = labelText.substr(0U, nul);
        }
        out << "{\"service_id\":" << s.serviceId
            << ",\"component_id\":" << s.componentId << ",\"label\":";
        appendJsonString(out, labelText);
        out << "}";
    }
    out << "]}";
    return out.str();
}

std::string serializeTunerErrorJson(const char* reason)
{
    return std::string("{\"status\":\"error\",\"reason\":\"") + reason + "\"}";
}

std::expected<TunerTuneRequest, ParseError> parseTunerTuneJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    const std::string_view band = extractJsonString(json, "band");
    if (band.empty()) {
        return std::unexpected(ParseError::MissingField);
    }

    TunerTuneRequest req = {};
    if (band == "dab") {
        req.band = TunerBand::Dab;
        unsigned long idx = 0U;
        if (!extractJsonUint(json, "freq_index", idx) || idx > 37U) {
            return std::unexpected(ParseError::MissingField);
        }
        req.dabFreqIndex = static_cast<std::uint8_t>(idx);
    } else if (band == "fm") {
        req.band = TunerBand::Fm;
        unsigned long khz = 0U;
        if (!extractJsonUint(json, "frequency_khz", khz)) {
            return std::unexpected(ParseError::MissingField);
        }
        auto freq = FrequencyKHz::tryFromKhz(static_cast<std::uint32_t>(khz));
        if (!freq) {
            return std::unexpected(freq.error());
        }
        req.fmFrequency = *freq;
    } else {
        return std::unexpected(ParseError::InvalidJson);
    }
    unsigned long antCap = 0U;
    if (extractJsonUint(json, "antcap", antCap) && antCap <= 128U) {
        req.antCap = static_cast<std::uint8_t>(antCap);
    }
    return req;
}

std::expected<TunerPlayRequest, ParseError> parseTunerPlayJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    unsigned long sid = 0U;
    unsigned long cid = 0U;
    if (!extractJsonUint(json, "service_id", sid)
        || !extractJsonUint(json, "component_id", cid)) {
        return std::unexpected(ParseError::MissingField);
    }

    TunerPlayRequest req = {};
    req.serviceId = static_cast<std::uint32_t>(sid);
    req.componentId = static_cast<std::uint32_t>(cid);
    return req;
}

std::expected<SeekDirection, ParseError> parseTunerSeekJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return SeekDirection::Up;
    }

    const std::string_view direction = extractJsonString(json, "direction");
    if (direction.empty()) {
        return SeekDirection::Up;
    }
    if (direction == "up") {
        return SeekDirection::Up;
    }
    if (direction == "down") {
        return SeekDirection::Down;
    }
    return std::unexpected(ParseError::InvalidJson);
}

std::expected<TunerScanRequest, ParseError> parseTunerScanJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    const std::string_view band = extractJsonString(json, "band");
    if (band.empty()) {
        return std::unexpected(ParseError::MissingField);
    }

    TunerScanRequest req = {};
    unsigned long maxSteps = 0U;
    if (band == "fm") {
        req.band = TunerBand::Fm;
        req.maxSteps = 45U;
    } else if (band == "dab") {
        req.band = TunerBand::Dab;
        req.maxSteps = 38U;
    } else {
        return std::unexpected(ParseError::InvalidJson);
    }

    if (extractJsonUint(json, "max_steps", maxSteps)) {
        if (maxSteps == 0U || maxSteps > 255U) {
            return std::unexpected(ParseError::InvalidJson);
        }
        req.maxSteps = static_cast<std::uint8_t>(maxSteps);
    }

    const std::string_view name = extractJsonString(json, "name");
    if (!name.empty()) {
        req.nameFilter.assign(name.begin(), name.end());
        for (char& ch : req.nameFilter) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
    }
    return req;
}

std::string serializeTunerScanJson(const TunerScanResult& result)
{
    std::ostringstream out;
    out << "{\"status\":\"" << (result.found ? "found" : "not_found") << '"'
        << ",\"band\":\"" << bandToken(result.band) << '"'
        << ",\"steps\":" << result.stepsTried;

    if (result.fmFrequency) {
        out << ",\"frequency_khz\":" << result.fmFrequency->value();
    }
    if (result.dabFreqIndex) {
        out << ",\"freq_index\":" << static_cast<unsigned>(*result.dabFreqIndex);
    }
    if (result.dabServiceId) {
        out << ",\"service_id\":" << *result.dabServiceId;
    }
    if (result.dabComponentId) {
        out << ",\"component_id\":" << *result.dabComponentId;
    }
    if (result.stationName) {
        out << ",\"station_name\":";
        appendJsonString(out, result.stationName->value());
    }
    out << '}';
    return out.str();
}

std::string serializeTunerFmBandScanJson(
    const std::vector<TunerFmScannedStation>& stations)
{
    std::ostringstream out;
    out << "{\"stations\":[";
    for (std::size_t i = 0; i < stations.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        const auto& s = stations[i];
        out << "{\"frequency_khz\":" << s.frequency.value()
            << ",\"rssi_dbuv\":" << static_cast<int>(s.rssiDbuV)
            << ",\"snr_db\":" << static_cast<int>(s.snrDb);
        appendOptionalLabel(out, "station_name", s.stationName);
        out << "}";
    }
    out << "]}";
    return out.str();
}

std::expected<AntennaCalibrationRequest, ParseError>
parseAntennaCalibrationJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    unsigned long antCap = 0U;
    if (!extractJsonUint(json, "antcap", antCap) || antCap > 128U) {
        return std::unexpected(ParseError::MissingField);
    }

    AntennaCalibrationRequest req = {};
    req.antCap = static_cast<std::uint8_t>(antCap);
    const std::string_view band = extractJsonString(json, "band");
    if (band == "dab") {
        req.band = TunerBand::Dab;
    } else if (band.empty() || band == "fm") {
        req.band = TunerBand::Fm;
    } else {
        return std::unexpected(ParseError::InvalidJson);
    }
    return req;
}

std::expected<XtalCalibrationRequest, ParseError>
parseXtalCalibrationJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    // Defaults match Si4684Driver::boot()'s own defaults -- a request that
    // only wants to change one parameter can omit the others.
    XtalCalibrationRequest req = {};
    req.ibias = 72U;
    req.ctun = 31U;
    req.xtalFreqHz = 19200000U;

    unsigned long ibias = 0U;
    if (extractJsonUint(json, "ibias", ibias)) {
        if (ibias > 127U) {
            return std::unexpected(ParseError::InvalidJson);
        }
        req.ibias = static_cast<std::uint8_t>(ibias);
    }

    unsigned long ctun = 0U;
    if (extractJsonUint(json, "ctun", ctun)) {
        if (ctun > 63U) {
            return std::unexpected(ParseError::InvalidJson);
        }
        req.ctun = static_cast<std::uint8_t>(ctun);
    }

    unsigned long xtalFreqHz = 0U;
    if (extractJsonUint(json, "xtal_freq_hz", xtalFreqHz)) {
        req.xtalFreqHz = static_cast<std::uint32_t>(xtalFreqHz);
    }

    return req;
}

} // namespace core
