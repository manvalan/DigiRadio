/**
 * @file    FallbackDspProgramSource.cpp
 * @brief   FallbackDspProgramSource implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "adau1701/FallbackDspProgramSource.hpp"

#include "esp_log.h"

namespace adau1701 {

namespace {
constexpr char kTag[] = "DspProgram";
} // namespace

FallbackDspProgramSource::FallbackDspProgramSource(
    core::IDspProgramSource& primary,
    core::IDspProgramSource& fallback)
    : primary_(primary)
    , fallback_(fallback)
{
}

std::expected<core::DspProgram, core::DspProgramError>
FallbackDspProgramSource::loadProgram()
{
    if (auto primary = primary_.loadProgram(); primary) {
        ESP_LOGI(kTag, "using flash DSP program");
        return primary;
    }

    ESP_LOGW(kTag, "flash DSP program unavailable — using embedded export");
    return fallback_.loadProgram();
}

} // namespace adau1701
