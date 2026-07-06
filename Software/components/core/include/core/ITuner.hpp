/**
 * @file    ITuner.hpp
 * @brief   Abstract tuner driver boundary (host-testable).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/FrequencyKHz.hpp"
#include "core/SeekDirection.hpp"
#include "core/TunerBand.hpp"
#include "core/TunerError.hpp"
#include "core/TunerStatus.hpp"

#include <cstdint>
#include <expected>
#include <vector>

namespace core {

/**
 * @brief    ITuner — hardware abstraction for DAB/FM tuning.
 *
 * @dname    ITuner
 * @return   n/a (type)
 * @pubstate Implemented by si4684::Si4684Tuner on device; fakes in host tests.
 *           No public data members.
 *
 * Services depend on this interface, not on SPI details. All methods return
 * std::expected with TunerError on failure.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class ITuner {
public:
    virtual ~ITuner() = default;

    /**
     * @brief    boot — load the requested band application image.
     *
     * @dname    boot
     * @param    band  DAB or FM image to load.
     * @return   Ok on success, or TunerError::HardwareFailed / NotBooted.
     * @pubstate none (implementation-defined driver state).
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, TunerError> boot(
        TunerBand band) = 0;

    /**
     * @brief    currentBand — read the loaded application band.
     *
     * @dname    currentBand
     * @return   Active TunerBand, or TunerError::NotBooted.
     * @pubstate reads driver boot state.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<TunerBand, TunerError> currentBand()
        const = 0;

    /**
     * @brief    readStatus — snapshot lock, metrics, and tune target.
     *
     * @dname    readStatus
     * @return   TunerStatus on success, or a TunerError.
     * @pubstate reads driver metrics; may refresh RSQ/DIGRAD.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<TunerStatus, TunerError> readStatus() = 0;

    /**
     * @brief    tuneDab — select a Band III ensemble by index.
     *
     * @dname    tuneDab
     * @param    freqIndex  Ensemble index 0–37.
     * @return   Ok on success, or WrongBand / TuneFailed / NotBooted.
     * @pubstate writes last tune target in the adapter.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, TunerError> tuneDab(
        std::uint8_t freqIndex) = 0;

    /**
     * @brief    tuneFm — tune to an FM centre frequency.
     *
     * @dname    tuneFm
     * @param    frequency  Validated FM centre frequency.
     * @return   Ok on success, or WrongBand / TuneFailed / NotBooted.
     * @pubstate writes last tune target in the adapter.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, TunerError> tuneFm(
        FrequencyKHz frequency) = 0;

    /**
     * @brief    seekFm — seek to the next valid FM station.
     *
     * @dname    seekFm
     * @param    direction  Up or Down scan direction.
     * @return   New centre frequency, or a TunerError.
     * @pubstate updates last tune target on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<FrequencyKHz, TunerError> seekFm(
        SeekDirection direction) = 0;

    /**
     * @brief    listDabServices — fetch programmes for the current ensemble.
     *
     * @dname    listDabServices
     * @return   Service entries, ServiceListEmpty, WrongBand, or NotBooted.
     * @pubstate reads DAB service list from the driver.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<std::vector<TunerServiceEntry>,
                                        TunerError>
    listDabServices() = 0;

    /**
     * @brief    playDabService — start DAB audio for a programme.
     *
     * @dname    playDabService
     * @param    serviceId    Selected service identifier.
     * @param    componentId  Audio component within the service.
     * @return   Ok on success, or WrongBand / HardwareFailed / NotBooted.
     * @pubstate starts digital audio output on the tuner I2S port.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, TunerError> playDabService(
        std::uint32_t serviceId, std::uint32_t componentId) = 0;

    /**
     * @brief    setVolume — set tuner output attenuation.
     *
     * @dname    setVolume
     * @param    level  Attenuator 0–63.
     * @return   Ok on success, or HardwareFailed / NotBooted.
     * @pubstate writes volume property on the driver.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, TunerError> setVolume(
        std::uint8_t level) = 0;
};

} // namespace core
