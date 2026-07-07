/**
 * @file    Si4684Tuner.cpp
 * @brief   Si4684Tuner implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "si4684/Si4684Tuner.hpp"

#include "si4684/Si4684Band.hpp"

namespace si4684 {

namespace {

[[nodiscard]] core::FrequencyKHz defaultFmFrequency()
{
    return *core::FrequencyKHz::tryFromKhz(101500U);
}

} // namespace

Si4684Tuner::Si4684Tuner(Si4684Driver& driver)
    : driver_(driver)
    , dabIndex_(0U)
    , fmFrequency_(defaultFmFrequency())
    , volume_(40U)
{
}

core::TunerError Si4684Tuner::mapError(Si4684Error error) noexcept
{
    switch (error) {
    case Si4684Error::NotBooted:
        return core::TunerError::NotBooted;
    case Si4684Error::WrongBand:
        return core::TunerError::WrongBand;
    case Si4684Error::TuneFailed:
    case Si4684Error::StcTimeout:
        return core::TunerError::TuneFailed;
    default:
        return core::TunerError::HardwareFailed;
    }
}

std::expected<void, core::TunerError> Si4684Tuner::boot(core::TunerBand band)
{
    const Si4684Band hwBand =
        (band == core::TunerBand::Fm) ? Si4684Band::Fm : Si4684Band::Dab;
    if (auto result = driver_.boot(hwBand); !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

std::expected<core::TunerBand, core::TunerError> Si4684Tuner::currentBand() const
{
    if (!driver_.isBooted()) {
        return std::unexpected(core::TunerError::NotBooted);
    }
    return driver_.loadedBand() == Si4684Band::Fm ? core::TunerBand::Fm
                                                  : core::TunerBand::Dab;
}

std::expected<core::TunerStatus, core::TunerError> Si4684Tuner::readStatus()
{
    if (!driver_.isBooted()) {
        return std::unexpected(core::TunerError::NotBooted);
    }

    core::TunerStatus status = {};
    status.booted = true;
    status.volume = volume_;
    status.band = driver_.loadedBand() == Si4684Band::Fm ? core::TunerBand::Fm
                                                         : core::TunerBand::Dab;

    if (status.band == core::TunerBand::Dab) {
        status.dabFreqIndex = dabIndex_;
        if (auto dig = driver_.readDabDigRadStatus(); dig) {
            status.locked = dig->valid;
            status.dabFicQuality = dig->ficQuality;
            status.dabCnrDb = dig->cnrDb;
        } else {
            return std::unexpected(mapError(dig.error()));
        }

        if (lastDabServiceId_) {
            if (auto data = driver_.readDabServiceData(false, true); data) {
                if (*data && (*data)->dataSrc == 2U) {
                    dabDynamicLabel_.applySegment((*data)->segmentIndex,
                                                  (*data)->segmentCount,
                                                  (*data)->payload);
                    status.dabDynamicLabel = dabDynamicLabel_.label();
                }
            } else {
                return std::unexpected(mapError(data.error()));
            }
        }
    } else {
        status.fmFrequency = fmFrequency_;
        if (auto rsq = driver_.readFmRsq(); rsq) {
            status.locked = rsq->valid;
            status.fmFrequency = rsq->frequency;
            status.fmRssiDbuV = rsq->rssiDbuV;
            status.fmSnrDb = rsq->snrDb;
            status.fmStereo = rsq->stereo;
            fmFrequency_ = rsq->frequency;
        } else {
            return std::unexpected(mapError(rsq.error()));
        }

        for (int attempt = 0; attempt < 8; ++attempt) {
            auto rds = driver_.readFmRds();
            if (!rds) {
                return std::unexpected(mapError(rds.error()));
            }
            if (!rds->received) {
                break;
            }
            rdsMetadata_.applyGroup(rds->blockA,
                                    rds->blockB,
                                    rds->blockC,
                                    rds->blockD);
            if (rds->fifoUsed <= 1U) {
                break;
            }
        }
        status.fmStationName = rdsMetadata_.programName();
        status.fmRadiotext = rdsMetadata_.radiotext();
    }
    return status;
}

std::expected<void, core::TunerError> Si4684Tuner::tuneDab(
    std::uint8_t freqIndex)
{
    if (auto result = driver_.tuneDab(freqIndex); !result) {
        return std::unexpected(mapError(result.error()));
    }
    dabIndex_ = freqIndex;
    dabDynamicLabel_.reset();
    lastDabServiceId_.reset();
    lastDabComponentId_.reset();
    return {};
}

std::expected<void, core::TunerError> Si4684Tuner::tuneFm(
    core::FrequencyKHz frequency)
{
    if (driver_.loadedBand() == Si4684Band::Dab && lastDabServiceId_
        && lastDabComponentId_) {
        (void)driver_.stopDabService(*lastDabServiceId_, *lastDabComponentId_);
        lastDabServiceId_.reset();
        lastDabComponentId_.reset();
        dabDynamicLabel_.reset();
    }

    if (auto result = driver_.tuneFm(frequency); !result) {
        return std::unexpected(mapError(result.error()));
    }
    fmFrequency_ = frequency;
    rdsMetadata_.reset();
    return {};
}

std::expected<core::FrequencyKHz, core::TunerError> Si4684Tuner::seekFm(
    core::SeekDirection direction)
{
    if (auto result = driver_.seekFm(direction, SeekBandWrap::Wrap); !result) {
        return std::unexpected(mapError(result.error()));
    }
    fmFrequency_ = *result;
    rdsMetadata_.reset();
    return *result;
}

std::expected<std::vector<core::TunerServiceEntry>, core::TunerError>
Si4684Tuner::listDabServices()
{
    if (auto events = driver_.readDabEventStatus(); events) {
        if (!events->serviceListReady) {
            return std::unexpected(core::TunerError::ServiceListEmpty);
        }
    } else {
        return std::unexpected(mapError(events.error()));
    }

    if (auto list = driver_.fetchDabServiceList(); list) {
        std::vector<core::TunerServiceEntry> out;
        out.reserve(list->size());
        for (const auto& item : *list) {
            core::TunerServiceEntry entry = {};
            entry.serviceId = item.serviceId;
            entry.componentId = item.componentId;
            entry.label = item.label;
            out.push_back(entry);
        }
        if (out.empty()) {
            return std::unexpected(core::TunerError::ServiceListEmpty);
        }
        return out;
    }
    return std::unexpected(mapError(list.error()));
}

std::expected<void, core::TunerError> Si4684Tuner::playDabService(
    std::uint32_t serviceId,
    std::uint32_t componentId)
{
    if (auto result = driver_.startDabService(serviceId, componentId); !result) {
        return std::unexpected(mapError(result.error()));
    }
    lastDabServiceId_ = serviceId;
    lastDabComponentId_ = componentId;
    dabDynamicLabel_.reset();
    return {};
}

std::expected<void, core::TunerError> Si4684Tuner::setVolume(std::uint8_t level)
{
    if (auto result = driver_.setVolume(level); !result) {
        return std::unexpected(mapError(result.error()));
    }
    volume_ = level & 0x3FU;
    return {};
}

} // namespace si4684
