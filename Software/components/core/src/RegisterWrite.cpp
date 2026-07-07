/**
 * @file    RegisterWrite.cpp
 * @brief   RegisterWrite implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/RegisterWrite.hpp"

namespace core {

RegisterWrite::RegisterWrite(std::uint16_t address,
                             std::vector<std::uint8_t> data)
    : address_(address)
    , data_(std::move(data))
{
}

std::uint16_t RegisterWrite::address() const noexcept
{
    return address_;
}

std::span<const std::uint8_t> RegisterWrite::data() const noexcept
{
    return data_;
}

} // namespace core
