/**
 * @file    IcyStreamFilter.cpp
 * @brief   Shoutcast/Icecast in-band metadata removal and playlist parsing.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/IcyStreamFilter.hpp"

#include <algorithm>

namespace core {

namespace {

constexpr std::string_view kTitleKey = "StreamTitle='";
constexpr std::size_t kMaxTitle = 128U;

[[nodiscard]] std::string_view trim(std::string_view text) noexcept
{
    while (!text.empty()
           && (text.front() == ' ' || text.front() == '\t'
               || text.front() == '\r')) {
        text.remove_prefix(1U);
    }
    while (!text.empty()
           && (text.back() == ' ' || text.back() == '\t'
               || text.back() == '\r')) {
        text.remove_suffix(1U);
    }
    return text;
}

[[nodiscard]] bool isStreamUrl(std::string_view text) noexcept
{
    return text.starts_with("http://") || text.starts_with("https://");
}

/** Length of the valid UTF-8 sequence starting at text[0], 0 if invalid. */
[[nodiscard]] std::size_t utf8SequenceLength(std::string_view text) noexcept
{
    const auto lead = static_cast<unsigned char>(text[0]);
    std::size_t length = 0U;
    if (lead >= 0xC2U && lead <= 0xDFU) {
        length = 2U;
    } else if (lead >= 0xE0U && lead <= 0xEFU) {
        length = 3U;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
        length = 4U;
    }
    if (length == 0U || text.size() < length) {
        return 0U;
    }
    for (std::size_t i = 1U; i < length; ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0U) != 0x80U) {
            return 0U;
        }
    }
    return length;
}

/** Appends one character, returning the input bytes it used. Control
 *  codes become spaces; bytes that are not valid UTF-8 are read as
 *  ISO-8859-1, which many Italian stations still send ("è" = 0xE8). The
 *  result is always valid UTF-8, which the app's JSON decoder requires. */
[[nodiscard]] std::size_t appendChar(std::string& out, std::string_view text)
{
    const auto byte = static_cast<unsigned char>(text[0]);
    if (byte < 0x80U) {
        out.push_back(byte < 0x20U || byte == 0x7FU ? ' ' : text[0]);
        return 1U;
    }
    if (const std::size_t length = utf8SequenceLength(text); length != 0U) {
        out.append(text.substr(0U, length));
        return length;
    }
    if (byte < 0xA0U) {
        out.push_back(' '); // C1 control range
        return 1U;
    }
    out.push_back(static_cast<char>(0xC0U | (byte >> 6U)));
    out.push_back(static_cast<char>(0x80U | (byte & 0x3FU)));
    return 1U;
}

[[nodiscard]] std::string printable(std::string_view text)
{
    std::string out;
    while (!text.empty() && out.size() < kMaxTitle) {
        text.remove_prefix(appendChar(out, text));
    }
    return std::string(trim(out));
}

[[nodiscard]] std::optional<std::string> plsEntry(std::string_view line)
{
    if (!line.starts_with("File")) {
        return std::nullopt;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view value = trim(line.substr(equals + 1U));
    return isStreamUrl(value) ? std::optional<std::string>(value)
                              : std::nullopt;
}

[[nodiscard]] std::optional<std::string> playlistLine(std::string_view line)
{
    line = trim(line);
    if (isStreamUrl(line)) {
        return std::string(line);
    }
    return plsEntry(line);
}

} // namespace

IcyStreamFilter::IcyStreamFilter(std::size_t metaInterval) noexcept
    : metaInterval_(metaInterval)
    , remaining_(metaInterval)
{
}

std::size_t IcyStreamFilter::filter(std::span<std::uint8_t> chunk)
{
    if (metaInterval_ == 0U) {
        return chunk.size();
    }
    std::size_t out = 0U;
    std::size_t in = 0U;
    while (in < chunk.size()) {
        if (phase_ != Phase::Audio) {
            consumeControlByte(chunk[in++]);
            continue;
        }
        const std::size_t run = std::min(remaining_, chunk.size() - in);
        std::copy_n(chunk.begin() + in, run, chunk.begin() + out);
        in += run;
        out += run;
        remaining_ -= run;
        if (remaining_ == 0U) {
            phase_ = Phase::Length;
        }
    }
    return out;
}

const std::optional<std::string>& IcyStreamFilter::title() const noexcept
{
    return title_;
}

void IcyStreamFilter::consumeControlByte(std::uint8_t byte)
{
    if (phase_ == Phase::Length) {
        remaining_ = static_cast<std::size_t>(byte) * 16U;
        metadata_.clear();
        phase_ = Phase::Metadata;
        if (remaining_ == 0U) {
            finishMetadata();
        }
        return;
    }
    metadata_.push_back(static_cast<char>(byte));
    if (--remaining_ == 0U) {
        finishMetadata();
    }
}

void IcyStreamFilter::finishMetadata()
{
    if (auto parsed = parseIcyStreamTitle(metadata_); parsed) {
        title_ = std::move(parsed);
    }
    metadata_.clear();
    remaining_ = metaInterval_;
    phase_ = Phase::Audio;
}

std::optional<std::string> parseIcyStreamTitle(std::string_view metadata)
{
    const std::size_t start = metadata.find(kTitleKey);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view rest = metadata.substr(start + kTitleKey.size());
    // Titles may contain apostrophes ("Guns N' Roses"): the value ends at
    // "';", falling back to the last quote for servers that omit the ';'.
    std::size_t end = rest.find("';");
    if (end == std::string_view::npos) {
        end = rest.rfind('\'');
    }
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    std::string title = printable(rest.substr(0U, end));
    if (title.empty()) {
        return std::nullopt;
    }
    return title;
}

std::optional<std::string> extractPlaylistStreamUrl(std::string_view body)
{
    while (!body.empty()) {
        const std::size_t newline = body.find('\n');
        const std::string_view line = body.substr(0U, newline);
        if (auto url = playlistLine(line); url) {
            return url;
        }
        if (newline == std::string_view::npos) {
            break;
        }
        body.remove_prefix(newline + 1U);
    }
    return std::nullopt;
}

bool isHlsPlaylist(std::string_view body) noexcept
{
    return body.find("#EXT-X-") != std::string_view::npos;
}

} // namespace core
