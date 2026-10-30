/**
 * @file    IcyStreamFilter.hpp
 * @brief   Shoutcast/Icecast in-band metadata removal and playlist parsing.
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
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    IcyStreamFilter — strips ICY metadata blocks out of an audio
 *           byte stream and keeps the latest StreamTitle.
 *
 * When a client sends `Icy-MetaData: 1`, the server answers with an
 * `icy-metaint: N` header and inserts, after every N audio bytes, one
 * length byte (×16) followed by that many bytes of text such as
 * `StreamTitle='Artist - Song';`. Left in, those bytes reach the MP3
 * decoder as corruption. The filter works in place on whatever chunk the
 * socket delivered, so blocks split across reads are handled.
 *
 * @dname    IcyStreamFilter
 * @pubstate tracks position within the audio/metadata cycle and the title.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class IcyStreamFilter {
public:
    /** Longest metadata block the protocol allows (255 × 16). */
    static constexpr std::size_t kMaxMetadata = 255U * 16U;

    /**
     * @brief    IcyStreamFilter — build a filter for one connection.
     *
     * @dname    IcyStreamFilter
     * @param    metaInterval  The icy-metaint header value; 0 = no metadata.
     * @pubstate starts at the beginning of an audio run.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    explicit IcyStreamFilter(std::size_t metaInterval) noexcept;

    /**
     * @brief    filter — remove metadata bytes from a chunk, in place.
     *
     * @dname    filter
     * @param    chunk  Bytes as read from the socket; compacted in place.
     * @return   Number of audio bytes left at the front of chunk.
     * @pubstate advances the cycle; may update the title.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::size_t filter(std::span<std::uint8_t> chunk);

    /**
     * @brief    title — the most recent non-empty StreamTitle.
     *
     * @dname    title
     * @return   Title text, or nullopt if none was received yet.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] const std::optional<std::string>& title() const noexcept;

private:
    enum class Phase { Audio, Length, Metadata };

    void consumeControlByte(std::uint8_t byte);
    void finishMetadata();

    std::size_t metaInterval_;
    std::size_t remaining_;
    Phase phase_{Phase::Audio};
    std::string metadata_;
    std::optional<std::string> title_;
};

/**
 * @brief    parseIcyStreamTitle — extract StreamTitle from a metadata block.
 *
 * @dname    parseIcyStreamTitle
 * @param    metadata  Raw block, e.g. "StreamTitle='A - B';StreamUrl='';".
 * @return   Printable title, or nullopt if absent or empty.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::optional<std::string> parseIcyStreamTitle(
    std::string_view metadata);

/**
 * @brief    extractPlaylistStreamUrl — first stream URL in an M3U or PLS.
 *
 * Station sites often link a playlist file rather than the stream. M3U:
 * the first non-comment line starting with http(s)://. PLS: the first
 * `FileN=` entry.
 *
 * @dname    extractPlaylistStreamUrl
 * @param    body  Playlist text (a few KB at most).
 * @return   The URL, or nullopt if none was found.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::optional<std::string> extractPlaylistStreamUrl(
    std::string_view body);

/**
 * @brief    isHlsPlaylist — whether a playlist is HLS (segments, not a
 *           continuous stream), which the firmware cannot play yet.
 *
 * @dname    isHlsPlaylist
 * @param    body  Playlist text.
 * @return   true when it carries HLS "EXT-X-" tags.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool isHlsPlaylist(std::string_view body) noexcept;

} // namespace core
