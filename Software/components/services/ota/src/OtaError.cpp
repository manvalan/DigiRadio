/**
 * @file    OtaError.cpp
 * @brief   OtaError token strings.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "ota/OtaError.hpp"

namespace ota {

const char* otaErrorToken(OtaError error) noexcept
{
    switch (error) {
    case OtaError::SessionActive:
        return "session_active";
    case OtaError::NoUpdatePartition:
        return "no_update_partition";
    case OtaError::BeginFailed:
        return "begin_failed";
    case OtaError::WriteFailed:
        return "write_failed";
    case OtaError::EndFailed:
        return "end_failed";
    case OtaError::ImageTooLarge:
        return "image_too_large";
    case OtaError::SetBootFailed:
        return "set_boot_failed";
    case OtaError::ConfirmFailed:
        return "confirm_failed";
    }
    return "unknown";
}

} // namespace ota
