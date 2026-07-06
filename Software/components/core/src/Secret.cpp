/**
 * @file    Secret.cpp
 * @brief   Secret implementation.
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

#include "core/Secret.hpp"

#include <cstring>

namespace core {

Secret::Secret(std::string value)
    : bytes_(std::move(value))
{
}

Secret::Secret(Secret&& other) noexcept
    : bytes_(std::move(other.bytes_))
{
    other.zeroize();
}

Secret& Secret::operator=(Secret&& other) noexcept
{
    if (this != &other) {
        zeroize();
        bytes_ = std::move(other.bytes_);
        other.zeroize();
    }
    return *this;
}

Secret::~Secret()
{
    zeroize();
}

std::size_t Secret::length() const noexcept
{
    return bytes_.size();
}

void Secret::zeroize() noexcept
{
    if (!bytes_.empty()) {
        std::memset(bytes_.data(), 0, bytes_.size());
        bytes_.clear();
    }
}

} // namespace core
