/**
 * @file    DspProgramBlob.cpp
 * @brief   DspProgram blob parse/serialise implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/DspProgramBlob.hpp"

#include <array>
#include <cstring>

namespace core {

namespace {

constexpr char kMagic[4] = {'D', 'R', 'A', 'D'};
constexpr std::uint16_t kVersion = 1U;
constexpr std::size_t kHeaderSize = 12U;
constexpr std::size_t kMaxBlobSize = 200U * 1024U;
constexpr std::size_t kMaxWriteCount = 32U;
constexpr std::size_t kMaxWritePayload = 16U * 1024U;

[[nodiscard]] std::uint16_t readLe16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

[[nodiscard]] std::uint32_t readLe32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0])
           | (static_cast<std::uint32_t>(p[1]) << 8)
           | (static_cast<std::uint32_t>(p[2]) << 16)
           | (static_cast<std::uint32_t>(p[3]) << 24);
}

[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> data)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

[[nodiscard]] bool magicMatches(std::span<const std::uint8_t> blob)
{
    return blob.size() >= 4U
           && blob[0] == static_cast<std::uint8_t>(kMagic[0])
           && blob[1] == static_cast<std::uint8_t>(kMagic[1])
           && blob[2] == static_cast<std::uint8_t>(kMagic[2])
           && blob[3] == static_cast<std::uint8_t>(kMagic[3]);
}

} // namespace

const char* dspProgramErrorToken(DspProgramError error) noexcept
{
    switch (error) {
    case DspProgramError::Empty:
        return "empty";
    case DspProgramError::Truncated:
        return "truncated";
    case DspProgramError::InvalidMagic:
        return "invalid_magic";
    case DspProgramError::UnsupportedVersion:
        return "unsupported_version";
    case DspProgramError::BadCrc:
        return "bad_crc";
    case DspProgramError::TooLarge:
        return "too_large";
    case DspProgramError::FlashReadFailed:
        return "flash_read_failed";
    case DspProgramError::FlashWriteFailed:
        return "flash_write_failed";
    }
    return "unknown";
}

std::expected<DspProgram, DspProgramError>
parseDspProgramBlob(std::span<const std::uint8_t> blob)
{
    if (blob.empty()) {
        return std::unexpected(DspProgramError::Empty);
    }
    if (blob.size() > kMaxBlobSize) {
        return std::unexpected(DspProgramError::TooLarge);
    }
    if (blob.size() < kHeaderSize) {
        return std::unexpected(DspProgramError::Truncated);
    }
    if (!magicMatches(blob)) {
        return std::unexpected(DspProgramError::InvalidMagic);
    }
    if (readLe16(blob.data() + 4U) != kVersion) {
        return std::unexpected(DspProgramError::UnsupportedVersion);
    }

    const std::uint16_t writeCount = readLe16(blob.data() + 6U);
    const std::uint32_t expectedCrc = readLe32(blob.data() + 8U);
    const std::span<const std::uint8_t> payload = blob.subspan(kHeaderSize);

    if (writeCount == 0U || writeCount > kMaxWriteCount) {
        return std::unexpected(DspProgramError::Truncated);
    }
    if (crc32(payload) != expectedCrc) {
        return std::unexpected(DspProgramError::BadCrc);
    }

    std::vector<RegisterWrite> writes;
    writes.reserve(writeCount);
    std::size_t offset = 0U;

    for (std::uint16_t index = 0U; index < writeCount; ++index) {
        if (offset + 4U > payload.size()) {
            return std::unexpected(DspProgramError::Truncated);
        }
        const std::uint16_t address =
            readLe16(payload.data() + offset);
        const std::uint16_t length =
            readLe16(payload.data() + offset + 2U);
        offset += 4U;

        if (length == 0U || length > kMaxWritePayload
            || offset + length > payload.size()) {
            return std::unexpected(DspProgramError::Truncated);
        }

        std::vector<std::uint8_t> data(length);
        std::memcpy(data.data(), payload.data() + offset, length);
        offset += length;
        writes.emplace_back(address, std::move(data));
    }

    if (offset != payload.size()) {
        return std::unexpected(DspProgramError::Truncated);
    }

    return DspProgram(std::move(writes));
}

std::vector<std::uint8_t> serializeDspProgramBlob(const DspProgram& program)
{
    std::vector<std::uint8_t> payload;
    for (const RegisterWrite& write : program.writes()) {
        const auto data = write.data();
        payload.push_back(static_cast<std::uint8_t>(write.address() & 0xFFU));
        payload.push_back(
            static_cast<std::uint8_t>((write.address() >> 8) & 0xFFU));
        const std::uint16_t length =
            static_cast<std::uint16_t>(data.size());
        payload.push_back(static_cast<std::uint8_t>(length & 0xFFU));
        payload.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFFU));
        payload.insert(payload.end(), data.begin(), data.end());
    }

    std::vector<std::uint8_t> blob;
    blob.reserve(kHeaderSize + payload.size());
    blob.insert(blob.end(), kMagic, kMagic + 4);
    blob.push_back(static_cast<std::uint8_t>(kVersion & 0xFFU));
    blob.push_back(static_cast<std::uint8_t>((kVersion >> 8) & 0xFFU));
    const std::uint16_t writeCount =
        static_cast<std::uint16_t>(program.writes().size());
    blob.push_back(static_cast<std::uint8_t>(writeCount & 0xFFU));
    blob.push_back(static_cast<std::uint8_t>((writeCount >> 8) & 0xFFU));
    const std::uint32_t payloadCrc = crc32(payload);
    blob.push_back(static_cast<std::uint8_t>(payloadCrc & 0xFFU));
    blob.push_back(static_cast<std::uint8_t>((payloadCrc >> 8) & 0xFFU));
    blob.push_back(static_cast<std::uint8_t>((payloadCrc >> 16) & 0xFFU));
    blob.push_back(static_cast<std::uint8_t>((payloadCrc >> 24) & 0xFFU));
    blob.insert(blob.end(), payload.begin(), payload.end());
    return blob;
}

} // namespace core
