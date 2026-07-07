/**
 * @file    OtaImageError.hpp
 * @brief   Failure causes for ESP32 firmware image header validation.
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

namespace core {

/**
 * @brief    OtaImageError — firmware image descriptor validation failures.
 *
 * @dname    OtaImageError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
enum class OtaImageError {
    InsufficientHeader, ///< Fewer bytes than the app descriptor offset.
    InvalidMagic,       ///< magic_word != ESP_APP_DESC_MAGIC_WORD.
    InvalidProject,     ///< project_name does not match this firmware tree.
};

} // namespace core
