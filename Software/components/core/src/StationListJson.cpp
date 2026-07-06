/**
 * @file    StationListJson.cpp
 * @brief   StationListJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/StationListJson.hpp"

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

[[nodiscard]] std::expected<TunerBand, ParseError>
parseBandField(std::string_view json)
{
    const std::string_view band = extractJsonString(json, "band");
    if (band == "fm") {
        return TunerBand::Fm;
    }
    if (band == "dab") {
        return TunerBand::Dab;
    }
    return std::unexpected(ParseError::MissingField);
}

[[nodiscard]] std::expected<Station, ParseError>
parseStationObject(std::string_view json)
{
    const std::string_view nameRaw = extractJsonString(json, "name");
    auto name = StationName::tryFrom(nameRaw);
    if (!name) {
        return std::unexpected(ParseError::MissingField);
    }

    auto band = parseBandField(json);
    if (!band) {
        return std::unexpected(ParseError::MissingField);
    }

    std::optional<PresetSlot> slot;
    unsigned long slotRaw = 0;
    if (extractJsonUint(json, "preset_slot", slotRaw)) {
        auto parsedSlot = PresetSlot::tryFrom(slotRaw);
        if (!parsedSlot) {
            return std::unexpected(ParseError::MissingField);
        }
        slot = *parsedSlot;
    }

    if (*band == TunerBand::Fm) {
        unsigned long khz = 0;
        if (!extractJsonUint(json, "fm_frequency_khz", khz)) {
            return std::unexpected(ParseError::MissingField);
        }
        auto frequency = FrequencyKHz::tryFromKhz(static_cast<std::uint32_t>(khz));
        if (!frequency) {
            return std::unexpected(ParseError::MissingField);
        }
        return Station(*name, TunerBand::Fm, 0U, std::nullopt, std::nullopt,
                       *frequency, slot);
    }

    unsigned long dabIndex = 0;
    if (!extractJsonUint(json, "dab_freq_index", dabIndex) || dabIndex > 37U) {
        return std::unexpected(ParseError::MissingField);
    }

    std::optional<std::uint32_t> serviceId;
    unsigned long serviceRaw = 0;
    if (extractJsonUint(json, "dab_service_id", serviceRaw)) {
        serviceId = static_cast<std::uint32_t>(serviceRaw);
    }

    std::optional<std::uint32_t> componentId;
    unsigned long componentRaw = 0;
    if (extractJsonUint(json, "dab_component_id", componentRaw)) {
        componentId = static_cast<std::uint32_t>(componentRaw);
    }

    return Station(*name, TunerBand::Dab, static_cast<std::uint8_t>(dabIndex),
                   serviceId, componentId, std::nullopt, slot);
}

void appendStationJson(std::ostringstream& out, const Station& station)
{
    out << "{\"name\":\"" << station.name().value() << "\",\"band\":\""
        << bandToken(station.band()) << "\"";

    if (station.band() == TunerBand::Fm && station.fmFrequency()) {
        out << ",\"fm_frequency_khz\":" << station.fmFrequency()->value();
    } else {
        out << ",\"dab_freq_index\":"
            << static_cast<unsigned>(station.dabFreqIndex());
        if (station.dabServiceId()) {
            out << ",\"dab_service_id\":" << *station.dabServiceId();
        }
        if (station.dabComponentId()) {
            out << ",\"dab_component_id\":" << *station.dabComponentId();
        }
    }

    if (station.presetSlot()) {
        out << ",\"preset_slot\":"
            << static_cast<unsigned>(station.presetSlot()->value());
    }
    out << '}';
}

} // namespace

std::string serializeStationListJson(const StationList& list)
{
    std::ostringstream out;
    out << "{\"stations\":[";
    bool first = true;
    for (const Station& station : list.stations()) {
        if (!first) {
            out << ',';
        }
        first = false;
        appendStationJson(out, station);
    }
    out << "]}";
    return out.str();
}

std::expected<StationList, ParseError> parseStationListJson(std::string_view json)
{
    StationList list;
    const std::size_t arrayStart = json.find("\"stations\"");
    if (arrayStart == std::string_view::npos) {
        return list;
    }

    std::size_t pos = json.find('[', arrayStart);
    if (pos == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    ++pos;

    while (pos < json.size()) {
        while (pos < json.size()
               && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\n'
                   || json[pos] == '\r')) {
            ++pos;
        }
        if (pos >= json.size() || json[pos] == ']') {
            break;
        }
        if (json[pos] != '{') {
            return std::unexpected(ParseError::InvalidJson);
        }
        const std::size_t objectStart = pos;
        int depth = 0;
        for (; pos < json.size(); ++pos) {
            if (json[pos] == '{') {
                ++depth;
            } else if (json[pos] == '}') {
                --depth;
                if (depth == 0) {
                    ++pos;
                    break;
                }
            }
        }
        const std::string_view object =
            json.substr(objectStart, pos - objectStart);
        auto station = parseStationObject(object);
        if (!station) {
            return std::unexpected(station.error());
        }
        if (auto added = list.add(std::move(*station)); !added) {
            return std::unexpected(ParseError::InvalidJson);
        }
    }

    return list;
}

std::expected<Station, ParseError> parseStationJson(std::string_view json)
{
    return parseStationObject(json);
}

std::expected<StationRemoveRequest, ParseError>
parseStationRemoveJson(std::string_view json)
{
    unsigned long index = 0;
    if (!extractJsonUint(json, "index", index)) {
        return std::unexpected(ParseError::MissingField);
    }
    return StationRemoveRequest{.index = static_cast<std::size_t>(index)};
}

std::string serializeStationListErrorJson(const char* reason)
{
    std::ostringstream out;
    out << "{\"status\":\"error\",\"reason\":\"" << reason << "\"}";
    return out.str();
}

const char* stationListErrorToken(StationListError error) noexcept
{
    switch (error) {
    case StationListError::Duplicate:
        return "duplicate";
    case StationListError::Full:
        return "full";
    case StationListError::NotFound:
        return "not_found";
    case StationListError::SlotInUse:
        return "slot_in_use";
    }
    return "station_error";
}

} // namespace core
