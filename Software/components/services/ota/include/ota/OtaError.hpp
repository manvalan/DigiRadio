/**
 * @file    OtaError.hpp
 * @brief   Failure causes for ESP32 OTA streaming updates.
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

namespace ota {

/**
 * @brief    OtaError — OTA session and flash write failures.
 *
 * @dname    OtaError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
enum class OtaError {
    SessionActive,
    NoUpdatePartition,
    BeginFailed,
    WriteFailed,
    EndFailed,
    ImageTooLarge,
    SetBootFailed,
    ConfirmFailed,
};

/**
 * @brief    otaErrorToken — stable API/JSON error string.
 *
 * @dname    otaErrorToken
 * @param    error  OTA failure from the service layer.
 * @return   Short token without secrets or image bytes.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] const char* otaErrorToken(OtaError error) noexcept;

} // namespace ota
