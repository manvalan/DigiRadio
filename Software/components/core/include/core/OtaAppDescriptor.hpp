/**
 * @file    OtaAppDescriptor.hpp
 * @brief   Host-testable validation of ESP-IDF app descriptors in OTA images.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include "core/OtaImageError.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace core {

/** Byte offset of esp_app_desc_t in a raw application .bin image. */
inline constexpr std::size_t kOtaAppDescriptorOffset = 0x20U;

/** Expected esp_app_desc_t::magic_word (ESP_APP_DESC_MAGIC_WORD). */
inline constexpr std::uint32_t kOtaAppDescriptorMagic = 0xABCD5432U;

/** Expected esp_app_desc_t::project_name for this tree (CMake project()). */
inline constexpr char kOtaProjectName[] = "digiradio";

/**
 * @brief    otaImageErrorToken — stable API/JSON error string.
 *
 * @dname    otaImageErrorToken
 * @param    error  Validation failure.
 * @return   Short token without secrets.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] const char* otaImageErrorToken(OtaImageError error) noexcept;

/**
 * @brief    validateOtaAppDescriptor — reject foreign or corrupt images.
 *
 * @dname    validateOtaAppDescriptor
 * @param    imagePrefix  First bytes of the incoming OTA stream (>= 0x20 + 72).
 * @return   Ok when magic and project_name match DigiRadio, else OtaImageError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<void, OtaImageError>
validateOtaAppDescriptor(std::span<const std::uint8_t> imagePrefix);

} // namespace core
