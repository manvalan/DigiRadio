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

#include "esp_log.h"
#include "si4684/Si4684Band.hpp"

namespace si4684 {

namespace {
constexpr char kTag[] = "Si4684Tuner";
constexpr std::uint32_t kFmSeekStepKhz = 100U;
constexpr std::uint32_t kFmBandBottomKhz = 87500U;
constexpr std::uint32_t kFmBandTopKhz = 107900U;

[[nodiscard]] core::FrequencyKHz defaultFmFrequency()
{
    return *core::FrequencyKHz::tryFromKhz(101500U);
}

} // namespace

Si4684Tuner::Si4684Tuner(Si4684Driver& driver)
    : driver_(driver)
    , dabIndex_(0U)
    , fmFrequency_(defaultFmFrequency())
    , volume_(63U)
    , fmBandReady_(false)
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
    case Si4684Error::CtsTimeout:
    case Si4684Error::CommandFailed:
        return core::TunerError::HardwareFailed;
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

std::expected<void, core::TunerError> Si4684Tuner::ensureBandLoaded(
    core::TunerBand band)
{
    const Si4684Band hwTarget =
        (band == core::TunerBand::Fm) ? Si4684Band::Fm : Si4684Band::Dab;
    if (driver_.isBooted() && driver_.loadedBand() == hwTarget) {
        if (hwTarget != Si4684Band::Fm || fmBandReady_) {
            return {};
        }
    }

    const bool reloading =
        !(driver_.isBooted() && driver_.loadedBand() == hwTarget);
    if (reloading) {
        ESP_LOGI(kTag, "band switch -> %s",
                 band == core::TunerBand::Fm ? "FM" : "DAB");
    }

    if (driver_.isBooted() && driver_.loadedBand() == Si4684Band::Dab
        && hwTarget == Si4684Band::Fm && lastDabServiceId_
        && lastDabComponentId_) {
        (void)driver_.stopDabService(*lastDabServiceId_, *lastDabComponentId_);
        lastDabServiceId_.reset();
        lastDabComponentId_.reset();
        dabDynamicLabel_.reset();
    }

    if (auto loaded = boot(band); !loaded) {
        return loaded;
    }

    if (auto vol = driver_.setVolume(volume_); !vol) {
        return std::unexpected(mapError(vol.error()));
    }

    if (band == core::TunerBand::Fm) {
        rdsMetadata_.reset();
        fmFrequency_ = defaultFmFrequency();
        fmBandReady_ = false;
        if (auto tuned = driver_.tuneFm(fmFrequency_); !tuned) {
            return std::unexpected(mapError(tuned.error()));
        }
        fmBandReady_ = true;
    } else {
        fmBandReady_ = false;
        dabDynamicLabel_.reset();
        lastDabServiceId_.reset();
        lastDabComponentId_.reset();
    }
    return {};
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
            status.fmRssiDbuV = rsq->rssiDbuV;
            status.fmSnrDb = rsq->snrDb;
            status.fmFreqOffBppm = rsq->freqOffBppm;
            status.fmStereo = rsq->stereo;
            status.fmChipReadFrequency = rsq->frequency;
            // Keep commanded frequency when chip READFREQ is stale (stuck at band
            // bottom); adopt READFREQ only when valid or it matches our target.
            status.fmFrequency = fmFrequency_;
            if (rsq->frequency) {
                const std::uint32_t chipKhz = rsq->frequency->value();
                if (rsq->valid || chipKhz == fmFrequency_.value()) {
                    status.fmFrequency = *rsq->frequency;
                    fmFrequency_ = *rsq->frequency;
                }
            }
        } else {
            return std::unexpected(mapError(rsq.error()));
        }

        for (int attempt = 0; attempt < 8; ++attempt) {
            auto rds = driver_.readFmRds();
            if (!rds) {
                ESP_LOGW(kTag, "FM RDS read failed (attempt %d)", attempt);
                break;
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
    std::uint8_t freqIndex, std::uint8_t antCap)
{
    if (auto ready = ensureBandLoaded(core::TunerBand::Dab); !ready) {
        return ready;
    }
    if (auto result = driver_.tuneDab(freqIndex, antCap); !result) {
        return std::unexpected(mapError(result.error()));
    }
    dabIndex_ = freqIndex;
    dabDynamicLabel_.reset();
    lastDabServiceId_.reset();
    lastDabComponentId_.reset();
    return {};
}

std::expected<void, core::TunerError> Si4684Tuner::tuneFm(
    core::FrequencyKHz frequency, std::uint8_t antCap)
{
    if (auto ready = ensureBandLoaded(core::TunerBand::Fm); !ready) {
        return ready;
    }
    if (auto result = driver_.tuneFm(frequency, antCap); !result) {
        return std::unexpected(mapError(result.error()));
    }
    fmFrequency_ = frequency;
    rdsMetadata_.reset();
    return {};
}

std::expected<core::FrequencyKHz, core::TunerError> Si4684Tuner::seekFm(
    core::SeekDirection direction)
{
    if (auto ready = ensureBandLoaded(core::TunerBand::Fm); !ready) {
        return std::unexpected(ready.error());
    }
    const core::FrequencyKHz before = fmFrequency_;
    const auto hw = driver_.seekFm(direction, SeekBandWrap::Wrap);
    if (hw && hw->value() != before.value()) {
        fmFrequency_ = *hw;
        rdsMetadata_.reset();
        return *hw;
    }

    // hitech95/si468x_dab_receiver uses HW seek + INTB STC; when READFREQ does
    // not move, step by FM_SEEK_FREQUENCY_SPACING (100 kHz) via FM_TUNE_FREQ.
    std::uint32_t nextKhz = before.value();
    if (direction == core::SeekDirection::Up) {
        nextKhz += kFmSeekStepKhz;
        if (nextKhz > kFmBandTopKhz) {
            nextKhz = kFmBandBottomKhz;
        }
    } else {
        nextKhz = (nextKhz > kFmBandBottomKhz + kFmSeekStepKhz)
                      ? nextKhz - kFmSeekStepKhz
                      : kFmBandTopKhz;
    }
    const auto next = core::FrequencyKHz::tryFromKhz(nextKhz);
    if (!next) {
        return std::unexpected(core::TunerError::TuneFailed);
    }
    if (auto stepped = tuneFm(*next); !stepped) {
        return std::unexpected(stepped.error());
    }
    if (auto rsq = driver_.readFmRsq(); rsq && rsq->frequency) {
        ESP_LOGI(kTag, "FM software seek %s -> %u kHz (chip READFREQ)",
                 direction == core::SeekDirection::Up ? "UP" : "DOWN",
                 static_cast<unsigned>(rsq->frequency->value()));
        return *rsq->frequency;
    }
    ESP_LOGI(kTag, "FM software seek %s -> %u kHz",
             direction == core::SeekDirection::Up ? "UP" : "DOWN",
             static_cast<unsigned>(nextKhz));
    return *next;
}

std::expected<std::vector<core::TunerServiceEntry>, core::TunerError>
Si4684Tuner::listDabServices()
{
    if (auto ready = ensureBandLoaded(core::TunerBand::Dab); !ready) {
        return std::unexpected(ready.error());
    }
    if (auto events = driver_.readDabEventStatus(); events) {
        if (!events->serviceListReady) {
            return std::unexpected(core::TunerError::ServiceListEmpty);
        }
    } else {
        return std::unexpected(mapError(events.error()));
    }

    const auto list = driver_.fetchDabServiceList();
    if (list) {
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
    if (auto ready = ensureBandLoaded(core::TunerBand::Dab); !ready) {
        return ready;
    }
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

std::expected<void, core::TunerError> Si4684Tuner::recalibrateXtal(
    std::uint8_t ibias, std::uint8_t ctun, std::uint32_t xtalFreqHz)
{
    if (auto result = driver_.recalibrateXtal(ibias, ctun, xtalFreqHz);
        !result) {
        return std::unexpected(mapError(result.error()));
    }
    return {};
}

} // namespace si4684
