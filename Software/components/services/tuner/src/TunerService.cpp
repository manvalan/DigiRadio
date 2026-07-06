/**
 * @file    TunerService.cpp
 * @brief   TunerService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "tuner/TunerService.hpp"

namespace tuner {

namespace {

[[nodiscard]] core::FrequencyKHz defaultFmFrequency()
{
    return *core::FrequencyKHz::tryFromKhz(101500U);
}

} // namespace

TunerService::TunerService(core::ITuner& tuner)
    : tuner_(tuner)
    , lastDabIndex_(0U)
    , lastFmFrequency_(defaultFmFrequency())
    , volume_(40U)
{
}

std::expected<core::TunerStatus, core::TunerError> TunerService::refreshStatus()
{
    auto status = tuner_.readStatus();
    if (status) {
        volume_ = status->volume;
        if (status->band == core::TunerBand::Dab) {
            status->dabPlayingServiceId = lastPlayedServiceId_;
            status->dabPlayingComponentId = lastPlayedComponentId_;
        }
    }
    return status;
}

std::expected<void, core::TunerError> TunerService::tuneDab(
    std::uint8_t freqIndex)
{
    if (auto result = tuner_.tuneDab(freqIndex); !result) {
        return result;
    }
    lastDabIndex_ = freqIndex;
    lastPlayedServiceId_.reset();
    lastPlayedComponentId_.reset();
    return {};
}

std::expected<void, core::TunerError> TunerService::tuneFm(
    core::FrequencyKHz frequency)
{
    if (auto result = tuner_.tuneFm(frequency); !result) {
        return result;
    }
    lastFmFrequency_ = frequency;
    return {};
}

std::expected<core::FrequencyKHz, core::TunerError> TunerService::seekFm(
    core::SeekDirection direction)
{
    auto result = tuner_.seekFm(direction);
    if (result) {
        lastFmFrequency_ = *result;
    }
    return result;
}

std::expected<std::vector<core::TunerServiceEntry>, core::TunerError>
TunerService::listDabServices()
{
    return tuner_.listDabServices();
}

std::expected<void, core::TunerError> TunerService::playDabService(
    std::uint32_t serviceId,
    std::uint32_t componentId)
{
    if (auto result = tuner_.playDabService(serviceId, componentId); !result) {
        return result;
    }
    lastPlayedServiceId_ = serviceId;
    lastPlayedComponentId_ = componentId;
    return {};
}

std::expected<void, core::TunerError> TunerService::setVolume(std::uint8_t level)
{
    if (auto result = tuner_.setVolume(level); !result) {
        return result;
    }
    volume_ = level & 0x3FU;
    return {};
}

core::ITuner& TunerService::tuner() noexcept
{
    return tuner_;
}

} // namespace tuner
