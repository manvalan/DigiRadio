/**
 * @file    Adau1701Dsp.cpp
 * @brief   Adau1701Dsp implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "adau1701/Adau1701Dsp.hpp"

namespace adau1701 {

Adau1701Dsp::Adau1701Dsp(Adau1701Driver& driver)
    : driver_(driver)
{
}

core::DspError Adau1701Dsp::mapError(Adau1701Error error) noexcept
{
    switch (error) {
    case Adau1701Error::NotBooted:
        return core::DspError::NotBooted;
    case Adau1701Error::InvalidParameter:
        return core::DspError::InvalidParameter;
    case Adau1701Error::SafeloadFailed:
        return core::DspError::SafeloadFailed;
    default:
        return core::DspError::SafeloadFailed;
    }
}

std::expected<void, core::DspError> Adau1701Dsp::applyProfile(
    const core::AudioProfile& profile)
{
    if (auto result = driver_.applyProfile(profile); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::selectSource(
    core::ActiveSource source)
{
    if (auto result = driver_.selectSource(source); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::applyEq(
    const core::EqProfile& eq)
{
    if (auto result = driver_.applyEq(eq); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::setMasterVolume(
    core::GainDb left, core::GainDb right)
{
    if (auto result = driver_.setMasterVolume(left, right); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::setEqBand(
    core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center,
    float q)
{
    if (auto result = driver_.setEqBand(band, gain, center, q); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::setBeepEnabled(bool enabled)
{
    if (auto result = driver_.setBeepEnabled(enabled); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::setBassBoostLevel(
    core::EnhanceLevel level)
{
    if (auto result = driver_.setBassBoostLevel(level); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::setStereoSpreadLevel(
    core::EnhanceLevel level)
{
    if (auto result = driver_.setStereoSpreadLevel(level); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<void, core::DspError> Adau1701Dsp::writeRawParam(
    unsigned address, float value)
{
    if (auto result = driver_.writeRawParam(address, value); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<core::AudioLevels, core::DspError> Adau1701Dsp::readLevels()
{
    auto result = driver_.readLevels();
    if (!result) {
        return std::unexpected(mapError(result.error()));
    }
    return *result;
}

} // namespace adau1701
