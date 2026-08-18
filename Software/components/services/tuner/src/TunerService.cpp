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

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace tuner {

namespace {
constexpr char kTag[] = "TunerSvc";
constexpr int kDabTuneSettleMs = 600;
/** FM scan step size (100 kHz spacing). */
constexpr std::uint32_t kFmScanStepKhz = 100U;
constexpr int kFmTuneSettleMs = 120;
constexpr int kFmNamePollMs = 250;
constexpr int kFmNamePollAttempts = 6;
constexpr std::int8_t kMinFmScanRssiDbuV = 5;
constexpr std::int8_t kMinFmScanSnrDb = 10;
/** Max |READFREQ − commanded| for scan hit without RDS lock (kHz). */
constexpr std::uint32_t kFmScanChipFreqSlackKhz = 150U;
constexpr std::uint8_t kMinDabFicQuality = 35U;
/** European FM band top used for scan/seek wrap (107.9 MHz). */
constexpr std::uint32_t kFmScanMaxKhz = 107900U;
/** European FM band bottom used for full-band scan (87.5 MHz). */
constexpr std::uint32_t kFmScanMinKhz = 87500U;

[[nodiscard]] core::FrequencyKHz fmScanStartFrequency(
    core::FrequencyKHz current,
    std::string_view nameFilter)
{
    if (!nameFilter.empty()) {
        return current;
    }
    const std::uint32_t khz = current.value();
    if (khz > kFmScanMaxKhz || khz < kFmScanMinKhz) {
        return *core::FrequencyKHz::tryFromKhz(kFmScanMinKhz);
    }
    return current;
}

[[nodiscard]] core::FrequencyKHz defaultFmFrequency()
{
    return *core::FrequencyKHz::tryFromKhz(101500U);
}

[[nodiscard]] std::string toLowerAscii(std::string_view text)
{
    std::string lowered;
    lowered.reserve(text.size());
    for (const char ch : text) {
        lowered.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(ch))));
    }
    return lowered;
}

[[nodiscard]] bool nameMatchesFilter(const std::optional<core::BroadcastLabel>& label,
                                     std::string_view filterLower)
{
    if (filterLower.empty()) {
        return true;
    }
    if (!label) {
        return false;
    }
    const std::string haystack = toLowerAscii(label->value());
    return haystack.find(filterLower) != std::string::npos;
}

[[nodiscard]] bool nameMatchesFilter(std::string_view label,
                                     std::string_view filterLower)
{
    if (filterLower.empty()) {
        return true;
    }
    const std::string haystack = toLowerAscii(label);
    return haystack.find(filterLower) != std::string::npos;
}

[[nodiscard]] bool fmStatusUsableForScan(const core::TunerStatus& status,
                                     std::uint32_t commandedKhz,
                                     bool requireLocked)
{
    if (!status.fmRssiDbuV || *status.fmRssiDbuV < kMinFmScanRssiDbuV) {
        return false;
    }
    if (requireLocked) {
        return status.locked;
    }
    if (!status.fmSnrDb || *status.fmSnrDb < kMinFmScanSnrDb) {
        return false;
    }
    if (status.fmChipReadFrequency) {
        const std::uint32_t chipKhz = status.fmChipReadFrequency->value();
        const std::uint32_t diff =
            chipKhz > commandedKhz ? chipKhz - commandedKhz
                                   : commandedKhz - chipKhz;
        if (diff > kFmScanChipFreqSlackKhz) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] core::TunerScanResult makeScanResult(
    const core::TunerScanRequest& request,
    std::uint16_t steps,
    bool found)
{
    core::TunerScanResult result = {};
    result.found = found;
    result.band = request.band;
    result.stepsTried = steps;
    return result;
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

std::expected<core::TunerScanResult, core::TunerError>
TunerService::scanForStation(const core::TunerScanRequest& request)
{
    const std::string_view nameFilter = request.nameFilter;

    if (request.band == core::TunerBand::Fm) {
        const core::FrequencyKHz scanStart =
            fmScanStartFrequency(lastFmFrequency_, nameFilter);
        if (auto tuned = tuneFm(scanStart); !tuned) {
            return std::unexpected(tuned.error());
        }

        const std::uint32_t startFreq = lastFmFrequency_.value();
        const bool requireLocked = !nameFilter.empty();
        std::uint32_t freqKhz = startFreq;

        ESP_LOGI(kTag, "FM scan start from %u kHz (max %u steps, name='%.*s')",
         static_cast<unsigned>(startFreq), static_cast<unsigned>(request.maxSteps),
         static_cast<int>(nameFilter.size()), nameFilter.data());

        for (std::uint16_t step = 1U; step <= request.maxSteps; ++step) {
            freqKhz += kFmScanStepKhz;
            if (freqKhz > kFmScanMaxKhz) {
                freqKhz = kFmScanMinKhz;
            }
            if (freqKhz == startFreq) {
                ESP_LOGI(kTag, "FM scan wrapped to start frequency");
                break;
            }

            const auto next = core::FrequencyKHz::tryFromKhz(freqKhz);
            if (!next) {
                return std::unexpected(core::TunerError::TuneFailed);
            }
            if (auto tuned = tuneFm(*next); !tuned) {
                ESP_LOGW(kTag, "FM scan step %u tune failed at %u kHz (err=%d)",
                    static_cast<unsigned>(step), static_cast<unsigned>(freqKhz),
                    static_cast<int>(tuned.error()));
                return std::unexpected(tuned.error());
            }
            vTaskDelay(pdMS_TO_TICKS(kFmTuneSettleMs));

            std::optional<core::BroadcastLabel> stationName;
            bool usable = false;
            for (int attempt = 0; attempt < kFmNamePollAttempts; ++attempt) {
                auto status = refreshStatus();
                if (!status) {
                    return std::unexpected(status.error());
                }
                if (attempt == 0) {
                    ESP_LOGI(kTag,
                             "FM scan step %u at %u kHz: valid=%d rssi=%d dBuV "
                             "snr=%d dB",
                             static_cast<unsigned>(step),
                             static_cast<unsigned>(lastFmFrequency_.value()),
                             static_cast<int>(status->locked),
                             status->fmRssiDbuV ? *status->fmRssiDbuV : -128,
                             status->fmSnrDb ? *status->fmSnrDb : -128);
                }
                if (!fmStatusUsableForScan(*status, freqKhz, requireLocked)) {
                    break;
                }
                usable = true;
                stationName = status->fmStationName;
                if (nameFilter.empty()
                    || nameMatchesFilter(stationName, nameFilter)) {
                    break;
                }
                if (attempt + 1 < kFmNamePollAttempts) {
                    vTaskDelay(pdMS_TO_TICKS(kFmNamePollMs));
                }
            }

            if (!usable) {
                continue;
            }

            if (nameFilter.empty() || nameMatchesFilter(stationName, nameFilter)) {
                auto status = refreshStatus();
                if (!status) {
                    return std::unexpected(status.error());
                }

                core::TunerScanResult result = makeScanResult(request, step, true);
                result.fmFrequency = status->fmFrequency;
                result.stationName = status->fmStationName;
                ESP_LOGI(kTag, "FM scan found %u kHz after %u steps",
                    static_cast<unsigned>(result.fmFrequency ? result.fmFrequency->value() : 0U),
                    static_cast<unsigned>(step));
                return result;
            }
        }

        ESP_LOGW(kTag, "FM scan: no station found");
        return makeScanResult(request, request.maxSteps, false);
    }

    ESP_LOGI(kTag, "DAB scan start (max %u ensembles, name='%.*s')",
             static_cast<unsigned>(request.maxSteps),
             static_cast<int>(nameFilter.size()), nameFilter.data());

    for (std::uint16_t step = 0U; step < request.maxSteps; ++step) {
        const std::uint8_t freqIndex = static_cast<std::uint8_t>(step);
        if (auto tuned = tuneDab(freqIndex); !tuned) {
            return std::unexpected(tuned.error());
        }
        vTaskDelay(pdMS_TO_TICKS(kDabTuneSettleMs));

        auto status = refreshStatus();
        if (!status) {
            return std::unexpected(status.error());
        }
        if (!status->locked || !status->dabFicQuality
            || *status->dabFicQuality < kMinDabFicQuality) {
            continue;
        }

        auto services = listDabServices();
        if (!services || services->empty()) {
            continue;
        }

        for (const core::TunerServiceEntry& service : *services) {
            const std::string_view label(service.label.data());
            if (!nameMatchesFilter(label, nameFilter)) {
                continue;
            }
            if (auto played =
                    playDabService(service.serviceId, service.componentId);
                !played) {
                continue;
            }

            core::TunerScanResult result =
                makeScanResult(request, static_cast<std::uint16_t>(step + 1U), true);
            result.dabFreqIndex = freqIndex;
            result.dabServiceId = service.serviceId;
            result.dabComponentId = service.componentId;
            if (auto parsed = core::BroadcastLabel::tryFromChipBytes(label);
                parsed) {
                result.stationName = std::move(*parsed);
            }
            ESP_LOGI(kTag, "DAB scan found idx=%u svc=%u after %u steps",
                     static_cast<unsigned>(freqIndex),
                     static_cast<unsigned>(service.serviceId),
                     static_cast<unsigned>(step + 1U));
            return result;
        }
    }

    ESP_LOGW(kTag, "DAB scan: no station found");
    return makeScanResult(request, request.maxSteps, false);
}

std::expected<std::vector<core::TunerFmScannedStation>, core::TunerError>
TunerService::scanFullFmBand()
{
    const auto bandBottom = *core::FrequencyKHz::tryFromKhz(kFmScanMinKhz);
    if (auto tuned = tuneFm(bandBottom); !tuned) {
        return std::unexpected(tuned.error());
    }

    std::vector<core::TunerFmScannedStation> stations;
    std::uint32_t previousKhz = kFmScanMinKhz;
    ESP_LOGI(kTag, "FM full band scan start from %u kHz",
             static_cast<unsigned>(kFmScanMinKhz));

    // One hardware seek per candidate; the FM band cannot hold more
    // stations than this at any legal channel spacing, so it is a safe
    // upper bound against seek ever failing to detect the wrap-around.
    constexpr int kMaxCandidates = 60;
    for (int i = 0; i < kMaxCandidates; ++i) {
        auto seeked = seekFm(core::SeekDirection::Up);
        if (!seeked) {
            return std::unexpected(seeked.error());
        }
        const std::uint32_t freqKhz = seeked->value();
        if (freqKhz <= previousKhz) {
            ESP_LOGI(kTag, "FM full band scan wrapped at %u kHz",
                     static_cast<unsigned>(freqKhz));
            break;
        }
        previousKhz = freqKhz;

        vTaskDelay(pdMS_TO_TICKS(kFmTuneSettleMs));
        auto status = refreshStatus();
        if (!status) {
            return std::unexpected(status.error());
        }
        if (!fmStatusUsableForScan(*status, freqKhz, /*requireLocked=*/true)) {
            continue;
        }

        std::optional<core::BroadcastLabel> stationName;
        for (int attempt = 0; attempt < kFmNamePollAttempts; ++attempt) {
            auto polled = refreshStatus();
            if (!polled) {
                break;
            }
            if (polled->fmStationName) {
                stationName = polled->fmStationName;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(kFmNamePollMs));
        }

        const core::TunerFmScannedStation entry{
            *status->fmFrequency,
            status->fmRssiDbuV.value_or(0),
            status->fmSnrDb.value_or(0),
            stationName,
        };
        ESP_LOGI(kTag, "FM full band scan hit: %u kHz rssi=%d snr=%d",
                 static_cast<unsigned>(freqKhz),
                 static_cast<int>(entry.rssiDbuV),
                 static_cast<int>(entry.snrDb));
        stations.push_back(entry);
    }
    return stations;
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
