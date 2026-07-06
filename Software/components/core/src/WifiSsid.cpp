/**
 * @file    WifiSsid.cpp
 * @brief   WifiSsid implementation.
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

#include "core/WifiSsid.hpp"

#include <cassert>

namespace core {

bool WifiSsid::isValid(std::string_view raw) noexcept
{
    return !raw.empty() && raw.size() <= kMaxLength;
}

WifiSsid::WifiSsid(std::string_view raw)
    : ssid_(raw)
{
    assert(isValid(raw));
}

std::string_view WifiSsid::value() const noexcept
{
    return ssid_;
}

} // namespace core
