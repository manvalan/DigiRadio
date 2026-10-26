/**
 * @file    icy_stream_filter_test.cpp
 * @brief   Host tests for IcyStreamFilter and playlist parsing.
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

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

/** Builds `cycles` rounds of 8 audio bytes ('a'+round) and one metadata
 *  block carrying `meta` (padded to a multiple of 16). */
[[nodiscard]] std::vector<std::uint8_t> buildStream(const std::string& meta,
                                                    int cycles)
{
    std::vector<std::uint8_t> stream;
    for (int c = 0; c < cycles; ++c) {
        stream.insert(stream.end(), 8U, static_cast<std::uint8_t>('a' + c));
        const std::size_t blocks = (meta.size() + 15U) / 16U;
        stream.push_back(static_cast<std::uint8_t>(c == 1 ? 0U : blocks));
        if (c != 1) {
            std::string padded = meta;
            padded.resize(blocks * 16U, '\0');
            stream.insert(stream.end(), padded.begin(), padded.end());
        }
    }
    return stream;
}

[[nodiscard]] int runSplitReadsTest()
{
    const auto stream =
        buildStream("StreamTitle='Guns N' Roses - Patience';", 3);
    core::IcyStreamFilter filter(8U);
    std::string audio;
    // Feed in awkward 5-byte reads so blocks straddle chunk boundaries.
    for (std::size_t i = 0U; i < stream.size(); i += 5U) {
        std::vector<std::uint8_t> chunk(
            stream.begin() + static_cast<long>(i),
            stream.begin() + static_cast<long>(std::min(stream.size(), i + 5U)));
        const std::size_t kept = filter.filter(chunk);
        audio.append(chunk.begin(), chunk.begin() + static_cast<long>(kept));
    }
    if (audio != "aaaaaaaabbbbbbbbcccccccc") {
        std::cerr << "audio mismatch: '" << audio << "'\n";
        return EXIT_FAILURE;
    }
    if (!filter.title() || *filter.title() != "Guns N' Roses - Patience") {
        std::cerr << "title mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTitleEncodingTest()
{
    // ISO-8859-1 "è" (0xE8) must come out as UTF-8 C3 A8; valid UTF-8
    // passes through; control codes become spaces.
    const auto latin = core::parseIcyStreamTitle("StreamTitle='Perch\xE8';");
    const auto utf8 =
        core::parseIcyStreamTitle("StreamTitle='Perch\xC3\xA8\x01x';");
    if (!latin || *latin != "Perch\xC3\xA8" || !utf8
        || *utf8 != "Perch\xC3\xA8 x") {
        std::cerr << "title encoding mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::parseIcyStreamTitle("StreamTitle='';")
        || core::parseIcyStreamTitle("StreamUrl='x';")) {
        std::cerr << "empty or missing title must be nullopt\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runPlaylistTest()
{
    const auto m3u = core::extractPlaylistStreamUrl(
        "#EXTM3U\r\n#EXTINF:-1,Radio\r\nhttp://host/stream.mp3\r\n");
    const auto pls = core::extractPlaylistStreamUrl(
        "[playlist]\nNumberOfEntries=1\nFile1=https://host:8000/live\n");
    if (!m3u || *m3u != "http://host/stream.mp3" || !pls
        || *pls != "https://host:8000/live") {
        std::cerr << "playlist URL mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::extractPlaylistStreamUrl("<html>nope</html>")
        || !core::isHlsPlaylist("#EXTM3U\n#EXT-X-VERSION:3\n")
        || core::isHlsPlaylist("#EXTM3U\nhttp://x\n")) {
        std::cerr << "playlist classification mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runNoMetadataTest()
{
    core::IcyStreamFilter filter(0U);
    std::vector<std::uint8_t> chunk(100U, 7U);
    if (filter.filter(chunk) != 100U || filter.title()) {
        std::cerr << "metaint 0 must pass everything through\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runSplitReadsTest() != EXIT_SUCCESS
        || runTitleEncodingTest() != EXIT_SUCCESS
        || runPlaylistTest() != EXIT_SUCCESS
        || runNoMetadataTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "icy_stream_filter_test OK\n";
    return EXIT_SUCCESS;
}
