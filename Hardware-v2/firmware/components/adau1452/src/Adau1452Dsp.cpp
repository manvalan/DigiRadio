/**
 * @file    Adau1452Dsp.cpp
 * @brief   Adau1452Dsp implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "adau1452/Adau1452Dsp.hpp"

namespace adau1452 {

namespace {

[[nodiscard]] core::DspError mapError(Adau1452Error error) noexcept
{
    switch (error) {
    case Adau1452Error::NotBooted:
    case Adau1452Error::CellsMissing:
    case Adau1452Error::ProgramMissing:
        return core::DspError::NotBooted;
    case Adau1452Error::InvalidParameter:
        return core::DspError::InvalidParameter;
    default:
        return core::DspError::SafeloadFailed;
    }
}

[[nodiscard]] std::expected<void, core::DspError> mapped(
    std::expected<void, Adau1452Error> result)
{
    if (!result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

} // namespace

std::expected<void, core::DspError> Adau1452Dsp::applyProfile(
    const core::AudioProfile& profile)
{
    return mapped(driver_.applyProfile(profile));
}

std::expected<void, core::DspError> Adau1452Dsp::selectSource(
    core::ActiveSource source)
{
    return mapped(driver_.selectSource(source));
}

std::expected<void, core::DspError> Adau1452Dsp::applyEq(
    const core::EqProfile& eq)
{
    return mapped(driver_.applyEq(eq));
}

std::expected<void, core::DspError> Adau1452Dsp::setMasterVolume(
    core::GainDb left, core::GainDb right)
{
    return mapped(driver_.setMasterVolume(left, right));
}

std::expected<void, core::DspError> Adau1452Dsp::setEqBand(
    core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center, float q)
{
    return mapped(driver_.setEqBand(band, gain, center, q));
}

std::expected<void, core::DspError> Adau1452Dsp::setBeepEnabled(bool enabled)
{
    return mapped(driver_.setBeepEnabled(enabled));
}

std::expected<void, core::DspError> Adau1452Dsp::setBassBoostLevel(
    core::EnhanceLevel level)
{
    return mapped(driver_.setBassBoostLevel(level));
}

std::expected<void, core::DspError> Adau1452Dsp::setStereoSpreadLevel(
    core::EnhanceLevel level)
{
    return mapped(driver_.setStereoSpreadLevel(level));
}

std::expected<void, core::DspError> Adau1452Dsp::writeRawParam(
    unsigned address, float value)
{
    return mapped(driver_.writeRawParam(address, value));
}

std::expected<core::AudioLevels, core::DspError> Adau1452Dsp::readLevels()
{
    auto levels = driver_.readLevels();
    if (!levels) {
        return std::unexpected(mapError(levels.error()));
    }
    return *levels;
}

} // namespace adau1452
