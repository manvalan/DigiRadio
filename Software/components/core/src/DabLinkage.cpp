/**
 * @file    DabLinkage.cpp
 * @brief   Parsers for AN649 DAB service-linking replies (0xB7, 0xC1, 0xBF).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "core/DabLinkage.hpp"

#include <cstddef>
#include <optional>

namespace core {

namespace {

/** AN649 0xBF: ID(4) + FREQ(4) + FREQ_INDEX(1) + RNM(1) + CONT(1) + CTRL(1). */
constexpr std::size_t kFiEntryBytes = 12U;
/** AN649 0xB7 per-set header: LSN(2) + flags(1) + NUM_LINKS(1). */
constexpr std::size_t kLinkSetHeaderBytes = 4U;
/** Plausible carrier range for FI entries: FM band to DAB Band III top. */
constexpr std::uint32_t kMinFiHz = 87500000U;
constexpr std::uint32_t kMaxFiHz = 240000000U;

[[nodiscard]] std::uint16_t le16(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint16_t>(b[at] | (b[at + 1U] << 8U));
}

[[nodiscard]] std::uint32_t le32(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint32_t>(b[at])
           | (static_cast<std::uint32_t>(b[at + 1U]) << 8U)
           | (static_cast<std::uint32_t>(b[at + 2U]) << 16U)
           | (static_cast<std::uint32_t>(b[at + 3U]) << 24U);
}

[[nodiscard]] DabLinkageSet linkSetHeader(std::span<const std::uint8_t> b,
                                          std::size_t at)
{
    const std::uint8_t flags = b[at + 2U];
    return DabLinkageSet{le16(b, at),
                         (flags & 0x80U) != 0U,
                         (flags & 0x02U) != 0U,
                         static_cast<DabLinkType>((flags >> 4U) & 0x03U),
                         {}};
}

[[nodiscard]] bool plausible(const DabFrequencyInfo& fi)
{
    // R&M is a 4-bit field; junk past the real list fails these checks.
    return fi.rnm <= 0x0FU && fi.frequencyHz >= kMinFiHz
           && fi.frequencyHz <= kMaxFiHz;
}

[[nodiscard]] std::optional<std::vector<DabFrequencyInfo>> readFiEntries(
    std::span<const std::uint8_t> b, std::size_t count)
{
    constexpr std::size_t kFirst = 4U; // after LENGTH_FI_LIST[31:0]
    if (count == 0U || kFirst + count * kFiEntryBytes > b.size()) {
        return std::nullopt;
    }
    std::vector<DabFrequencyInfo> out;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = kFirst + i * kFiEntryBytes;
        const DabFrequencyInfo fi{le32(b, at), le32(b, at + 4U), b[at + 8U],
                                  b[at + 9U]};
        if (!plausible(fi)) {
            return std::nullopt;
        }
        out.push_back(fi);
    }
    return out;
}

} // namespace

std::expected<std::vector<DabLinkageSet>, ParseError>
parseDabServiceLinking(std::span<const std::uint8_t> resp)
{
    if (resp.size() < 4U) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::size_t setCount = resp[2];
    std::vector<DabLinkageSet> sets;
    std::size_t at = 4U; // SIZE(2) + NUM_LINKSETS(1) + reserved(1)
    for (std::size_t s = 0; s < setCount; ++s) {
        if (at + kLinkSetHeaderBytes > resp.size()) {
            return std::unexpected(ParseError::InvalidJson);
        }
        DabLinkageSet set = linkSetHeader(resp, at);
        const std::size_t links = resp[at + 3U];
        at += kLinkSetHeaderBytes;
        if (at + links * 4U > resp.size()) {
            return std::unexpected(ParseError::InvalidJson);
        }
        for (std::size_t l = 0; l < links; ++l, at += 4U) {
            set.ids.push_back(le32(resp, at));
        }
        sets.push_back(std::move(set));
    }
    return sets;
}

std::expected<std::vector<std::uint16_t>, ParseError>
parseDabOeServices(std::span<const std::uint8_t> resp)
{
    if (resp.size() < 4U) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::size_t count = resp[2];
    if (4U + count * 2U > resp.size()) {
        return std::unexpected(ParseError::InvalidJson);
    }
    std::vector<std::uint16_t> eids;
    for (std::size_t i = 0; i < count; ++i) {
        eids.push_back(le16(resp, 4U + i * 2U));
    }
    return eids;
}

std::expected<std::vector<DabFrequencyInfo>, ParseError>
parseDabFreqInfo(std::span<const std::uint8_t> resp)
{
    if (resp.size() < 4U) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::uint32_t length = le32(resp, 0U);
    if (length == 0U) {
        return std::vector<DabFrequencyInfo>{};
    }
    if (auto asCount = readFiEntries(resp, length); asCount) {
        return *asCount;
    }
    if (length % kFiEntryBytes == 0U) {
        if (auto asBytes = readFiEntries(resp, length / kFiEntryBytes);
            asBytes) {
            return *asBytes;
        }
    }
    return std::unexpected(ParseError::InvalidJson);
}

} // namespace core
