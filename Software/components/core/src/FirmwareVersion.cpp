/**
 * @file    FirmwareVersion.cpp
 * @brief   FirmwareVersion implementation.
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
 * @date    2026-07-06
 */

#include "core/FirmwareVersion.hpp"

#include <cassert>

namespace core {

FirmwareVersion::FirmwareVersion(std::string_view version)
    : version_(version)
{
    assert(!version.empty());
}

std::string_view FirmwareVersion::value() const noexcept
{
    return version_;
}

} // namespace core
