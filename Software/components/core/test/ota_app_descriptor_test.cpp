/**
 * @file    ota_app_descriptor_test.cpp
 * @brief   Host tests for OTA app descriptor validation.
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

#include <array>
#include <cassert>
#include <cstring>
#include <cstdint>

namespace {

void writeLe32(std::uint8_t* p, std::uint32_t value)
{
    p[0] = static_cast<std::uint8_t>(value);
    p[1] = static_cast<std::uint8_t>(value >> 8);
    p[2] = static_cast<std::uint8_t>(value >> 16);
    p[3] = static_cast<std::uint8_t>(value >> 24);
}

std::array<std::uint8_t, 256> makeValidPrefix()
{
    std::array<std::uint8_t, 256> image{};
    writeLe32(image.data() + core::kOtaAppDescriptorOffset,
              core::kOtaAppDescriptorMagic);
    std::memcpy(image.data() + core::kOtaAppDescriptorOffset + 40U,
                core::kOtaProjectName,
                std::strlen(core::kOtaProjectName));
    return image;
}

void testValidDescriptor()
{
    const auto image = makeValidPrefix();
    const auto result = core::validateOtaAppDescriptor(image);
    assert(result.has_value());
}

void testInsufficientHeader()
{
    const std::array<std::uint8_t, 32> shortImage{};
    const auto result = core::validateOtaAppDescriptor(shortImage);
    assert(!result.has_value());
    assert(result.error() == core::OtaImageError::InsufficientHeader);
}

void testInvalidMagic()
{
    auto image = makeValidPrefix();
    writeLe32(image.data() + core::kOtaAppDescriptorOffset, 0U);
    const auto result = core::validateOtaAppDescriptor(image);
    assert(!result.has_value());
    assert(result.error() == core::OtaImageError::InvalidMagic);
}

void testInvalidProject()
{
    auto image = makeValidPrefix();
    const char foreign[] = "other-project";
    std::memcpy(image.data() + core::kOtaAppDescriptorOffset + 40U,
                foreign,
                sizeof(foreign));
    const auto result = core::validateOtaAppDescriptor(image);
    assert(!result.has_value());
    assert(result.error() == core::OtaImageError::InvalidProject);
}

} // namespace

int main()
{
    testValidDescriptor();
    testInsufficientHeader();
    testInvalidMagic();
    testInvalidProject();
    return 0;
}
