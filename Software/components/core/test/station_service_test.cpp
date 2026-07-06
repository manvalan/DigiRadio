/**
 * @file    station_service_test.cpp
 * @brief   Host tests for StationService with in-memory fake store.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/FrequencyKHz.hpp"
#include "core/ISecureStore.hpp"
#include "core/StationListJson.hpp"
#include "core/StationName.hpp"
#include "core/TunerBand.hpp"
#include "station/StationService.hpp"
#include "tuner/TunerService.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

class FakeTuner final : public core::ITuner {
public:
    [[nodiscard]] std::expected<void, core::TunerError> boot(
        core::TunerBand) override
    {
        return {};
    }

    [[nodiscard]] std::expected<core::TunerBand, core::TunerError> currentBand()
        const override
    {
        return core::TunerBand::Dab;
    }

    [[nodiscard]] std::expected<core::TunerStatus, core::TunerError> readStatus()
        override
    {
        core::TunerStatus status = {};
        status.booted = true;
        status.band = core::TunerBand::Dab;
        status.locked = true;
        status.volume = 40;
        status.dabFreqIndex = 12U;
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

private:
    std::optional<std::string> stationJson_;
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

[[nodiscard]] int runPersistRoundTripTest()
{
    FakeSecureStore store;
    FakeTuner tuner;
    tuner::TunerService tunerService(tuner);
    station::StationService service(store, tunerService);

    if (auto added = service.add(makeFmStation("Jazz", 101500U)); !added) {
        std::cerr << "add failed\n";
        return EXIT_FAILURE;
    }

    station::StationService reloaded(store, tunerService);
    if (auto loaded = reloaded.loadFromStore(); !loaded) {
        std::cerr << "load failed\n";
        return EXIT_FAILURE;
    }
    if (reloaded.list().stations().size() != 1U) {
        std::cerr << "expected one preset after reload\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runReorderPersistTest()
{
    FakeSecureStore store;
    FakeTuner tuner;
    tuner::TunerService tunerService(tuner);
    station::StationService service(store, tunerService);

    if (auto a = service.add(makeFmStation("A", 88000U)); !a) {
        return EXIT_FAILURE;
    }
    if (auto b = service.add(makeFmStation("B", 90000U)); !b) {
        return EXIT_FAILURE;
    }
    if (auto moved = service.reorder(1U, 0U); !moved) {
        std::cerr << "reorder failed\n";
        return EXIT_FAILURE;
    }

    station::StationService reloaded(store, tunerService);
    if (auto loaded = reloaded.loadFromStore(); !loaded) {
        return EXIT_FAILURE;
    }
    if (reloaded.list().stations()[0U].name().value() != "B") {
        std::cerr << "reorder not persisted\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runPersistRoundTripTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runReorderPersistTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
