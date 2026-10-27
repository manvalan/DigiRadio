/**
 * @file    WebRadioBudget.cpp
 * @brief   Web radio ring sizing.
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
 * @date    2026-10-27
 */

#include "core/WebRadioBudget.hpp"

namespace core {

std::optional<std::size_t> chooseWebRadioRing(std::size_t freeBytes,
                                              std::size_t largestBlock,
                                              bool https) noexcept
{
    const std::size_t headroom =
        kWebRadioHttpHeadroom + (https ? kWebRadioTlsHeadroom : 0U);
    for (const std::size_t ring : kWebRadioRingSizes) {
        if (ring <= largestBlock && ring + headroom <= freeBytes) {
            return ring;
        }
    }
    return std::nullopt;
}

} // namespace core
