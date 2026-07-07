/**
 * @file    OtaAppDescriptor.cpp
 * @brief   OTA app descriptor validation implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/OtaAppDescriptor.hpp"

#include <cstring>
#include <expected>

namespace core {

namespace {

constexpr std::size_t kMinProjectNameCheck =
    kOtaAppDescriptorOffset + 4U + 4U + 32U + 32U;

[[nodiscard]] std::uint32_t readLe32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0])
           | (static_cast<std::uint32_t>(p[1]) << 8)
           | (static_cast<std::uint32_t>(p[2]) << 16)
           | (static_cast<std::uint32_t>(p[3]) << 24);
}

[[nodiscard]] bool projectNameMatches(const char* field)
{
    return std::strncmp(field, kOtaProjectName, sizeof(kOtaProjectName) - 1U)
           == 0;
}

} // namespace

const char* otaImageErrorToken(OtaImageError error) noexcept
{
    switch (error) {
    case OtaImageError::InsufficientHeader:
        return "insufficient_header";
    case OtaImageError::InvalidMagic:
        return "invalid_magic";
    case OtaImageError::InvalidProject:
        return "invalid_project";
    }
    return "unknown";
}

std::expected<void, OtaImageError>
validateOtaAppDescriptor(std::span<const std::uint8_t> imagePrefix)
{
    if (imagePrefix.size() < kMinProjectNameCheck) {
        return std::unexpected(OtaImageError::InsufficientHeader);
    }

    const std::uint8_t* desc = imagePrefix.data() + kOtaAppDescriptorOffset;
    const std::uint32_t magic = readLe32(desc);
    if (magic != kOtaAppDescriptorMagic) {
        return std::unexpected(OtaImageError::InvalidMagic);
    }

    const char* projectName =
        reinterpret_cast<const char*>(desc + 4U + 4U + 32U);
    if (!projectNameMatches(projectName)) {
        return std::unexpected(OtaImageError::InvalidProject);
    }

    return {};
}

} // namespace core
