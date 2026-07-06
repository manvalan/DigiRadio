/**
 * @file    integration_service_test.cpp
 * @brief   Host tests for IntegrationService preset recall flow.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "audio/AudioService.hpp"
#include "core/FrequencyKHz.hpp"
#include "core/IDsp.hpp"
#include "core/ISecureStore.hpp"
#include "core/StationName.hpp"
#include "core/TunerBand.hpp"
#include "integration/IntegrationService.hpp"
#include "station/StationService.hpp"
#include "tuner/TunerService.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

class FakeDsp final : public core::IDsp {
public:
    std::size_t applyProfileCalls{0U};

    [[nodiscard]] std::expected<void, core::DspError> applyProfile(
        const core::AudioProfile&) override
    {
        ++applyProfileCalls;
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> applyMixer(
        const core::MixerState&) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> applyEq(
        const core::EqProfile&) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setInputVolume(
        core::MixSource, core::GainDb, core::GainDb) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setMasterVolume(
        core::GainDb, core::GainDb) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setEqBand(
        core::EqBandIndex, core::GainDb, core::FrequencyHz, float) override
    {
        return {};
    }
};

class TrackingTuner final : public core::ITuner {
public:
    bool tunedFm{false};

    [[nodiscard]] std::expected<void, core::TunerError> boot(
        core::TunerBand) override
    {
        return {};
    }

    [[nodiscard]] std::expected<core::TunerBand, core::TunerError> currentBand()
        const override
    {
        return core::TunerBand::Fm;
    }

    [[nodiscard]] std::expected<core::TunerStatus, core::TunerError> readStatus()
        override
    {
        core::TunerStatus status = {};
        status.booted = true;
        status.band = core::TunerBand::Fm;
        status.locked = true;
        status.volume = 40;
        return status;
    }

    [[nodiscard]] std::expected<void, core::TunerError> tuneDab(
        std::uint8_t) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::TunerError> tuneFm(
        core::FrequencyKHz) override
    {
        tunedFm = true;
        return {};
    }

    [[nodiscard]] std::expected<core::FrequencyKHz, core::TunerError> seekFm(
        core::SeekDirection) override
    {
        return std::unexpected(core::TunerError::WrongBand);
    }

    [[nodiscard]] std::expected<std::vector<core::TunerServiceEntry>,
                                core::TunerError>
    listDabServices() override
    {
        return std::vector<core::TunerServiceEntry>{};
    }

    [[nodiscard]] std::expected<void, core::TunerError> playDabService(
        std::uint32_t, std::uint32_t) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::TunerError> setVolume(
        std::uint8_t) override
    {
        return {};
    }
};

class FakeSecureStore final : public core::ISecureStore {
public:
    [[nodiscard]] bool hasWifiCredentials() const override
    {
        return false;
    }

    [[nodiscard]] std::expected<void, core::StoreError> saveWifiCredentials(
        const core::WifiCredentials&) override
    {
        return std::unexpected(core::StoreError::IoFailed);
    }

    [[nodiscard]] std::expected<core::WifiCredentials, core::StoreError>
    loadWifiCredentials() const override
    {
        return std::unexpected(core::StoreError::NotFound);
    }

    [[nodiscard]] std::expected<void, core::StoreError> clearWifiCredentials()
        override
    {
        return {};
    }

    [[nodiscard]] bool hasStationList() const override
    {
        return stationJson_.has_value();
    }

    [[nodiscard]] std::expected<void, core::StoreError> saveStationListJson(
        std::string_view json) override
    {
        stationJson_ = std::string(json);
        return {};
    }

    [[nodiscard]] std::expected<std::string, core::StoreError>
    loadStationListJson() const override
    {
        if (!stationJson_) {
            return std::unexpected(core::StoreError::NotFound);
        }
        return *stationJson_;
    }

    [[nodiscard]] std::expected<void, core::StoreError> clearStationList()
        override
    {
        stationJson_.reset();
        return {};
    }

    [[nodiscard]] bool hasLastPresetIndex() const override
    {
        return lastPresetIndex_.has_value();
    }

    [[nodiscard]] std::expected<void, core::StoreError> saveLastPresetIndex(
        std::uint8_t index) override
    {
        lastPresetIndex_ = index;
        return {};
    }

    [[nodiscard]] std::expected<std::uint8_t, core::StoreError>
    loadLastPresetIndex() const override
    {
        if (!lastPresetIndex_) {
            return std::unexpected(core::StoreError::NotFound);
        }
        return *lastPresetIndex_;
    }

    [[nodiscard]] std::expected<void, core::StoreError> clearLastPresetIndex()
        override
    {
        lastPresetIndex_.reset();
        return {};
    }

private:
    std::optional<std::string> stationJson_;
    std::optional<std::uint8_t> lastPresetIndex_;
};

[[nodiscard]] core::Station makeFmStation(const char* name, std::uint32_t khz)
{
    return core::Station(
        *core::StationName::tryFrom(name),
        core::TunerBand::Fm,
        0U,
        std::nullopt,
        std::nullopt,
        *core::FrequencyKHz::tryFromKhz(khz),
        std::nullopt);
}

[[nodiscard]] int runRecallPresetTest()
{
    FakeSecureStore store;
    TrackingTuner tunerHw;
    FakeDsp dsp;
    tuner::TunerService tunerService(tunerHw);
    audio::AudioService audioService(dsp, nullptr);
    station::StationService stationService(store, tunerService);
    integration::IntegrationService integration(
        store, tunerService, audioService, stationService);

    if (auto added = stationService.add(makeFmStation("Jazz", 101500U)); !added) {
        std::cerr << "add preset failed\n";
        return EXIT_FAILURE;
    }

    if (auto recalled = integration.recallPreset(0U); !recalled) {
        std::cerr << "recallPreset failed\n";
        return EXIT_FAILURE;
    }
    if (!tunerHw.tunedFm || dsp.applyProfileCalls == 0U
        || !store.hasLastPresetIndex()) {
        std::cerr << "recallPreset side effects missing\n";
        return EXIT_FAILURE;
    }

    tunerHw.tunedFm = false;
    dsp.applyProfileCalls = 0U;
    integration::IntegrationService rebooted(
        store, tunerService, audioService, stationService);
    if (auto started = rebooted.startup(); !started) {
        std::cerr << "startup failed\n";
        return EXIT_FAILURE;
    }
    if (!tunerHw.tunedFm || dsp.applyProfileCalls == 0U) {
        std::cerr << "startup did not recall last preset\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runRecallPresetTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
