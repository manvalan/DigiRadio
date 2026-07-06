/**
 * @file    Si4684Tuner.hpp
 * @brief   core::ITuner adapter over Si4684Driver.
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
#include "core/ITuner.hpp"
#include "si4684/Si4684Driver.hpp"

namespace si4684 {

/**
 * @brief    Si4684Tuner — maps Si4684Driver to core::ITuner.
 *
 * @dname    Si4684Tuner
 * @param    driver  Borrowed Si4684Driver (must outlive this adapter).
 * @return   n/a (type)
 * @pubstate Borrows driver_. Caches last DAB index, FM frequency, and volume
 *           for status reporting. No public data members.
 *
 * Translates domain calls into SPI commands and maps Si4684Error to
 * core::TunerError at this boundary.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Si4684Tuner : public core::ITuner {
public:
    /**
     * @brief    Si4684Tuner — bind to a booted or bootable driver instance.
     *
     * @dname    Si4684Tuner
     * @param    driver  Si4684 driver constructed by HardwareBootstrap.
     * @pubstate stores driver reference; sets default tune targets.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit Si4684Tuner(Si4684Driver& driver);

    /**
     * @brief    boot — load the requested Si4684 application image.
     *
     * @dname    boot
     * @param    band  DAB or FM image to load.
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate delegates to driver_.boot().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> boot(
        core::TunerBand band) override;

    /**
     * @brief    currentBand — read the loaded application band.
     *
     * @dname    currentBand
     * @return   Active TunerBand, or TunerError::NotBooted.
     * @pubstate reads driver_.loadedBand().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::TunerBand, core::TunerError> currentBand()
        const override;

    /**
     * @brief    readStatus — build a core TunerStatus from driver metrics.
     *
     * @dname    readStatus
     * @return   TunerStatus on success, or a mapped TunerError.
     * @pubstate reads driver_; refreshes cached tune targets from RSQ/DIGRAD.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::TunerStatus, core::TunerError> readStatus()
        override;

    /**
     * @brief    tuneDab — tune to a Band III ensemble index.
     *
     * @dname    tuneDab
     * @param    freqIndex  Ensemble index 0–37.
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate writes dabIndex_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneDab(
        std::uint8_t freqIndex) override;

    /**
     * @brief    tuneFm — tune to an FM centre frequency in kHz.
     *
     * @dname    tuneFm
     * @param    frequency  Validated FM centre frequency.
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate writes fmFrequency_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneFm(
        core::FrequencyKHz frequency) override;

    /**
     * @brief    seekFm — seek FM with band wrap.
     *
     * @dname    seekFm
     * @param    direction  Up or Down scan direction.
     * @return   New centre frequency, or a mapped TunerError.
     * @pubstate writes fmFrequency_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::FrequencyKHz, core::TunerError> seekFm(
        core::SeekDirection direction) override;

    /**
     * @brief    listDabServices — fetch programmes for the current ensemble.
     *
     * @dname    listDabServices
     * @return   Service entries, ServiceListEmpty, or a mapped TunerError.
     * @pubstate reads driver_ service list when ready.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<std::vector<core::TunerServiceEntry>,
                                    core::TunerError>
    listDabServices() override;

    /**
     * @brief    playDabService — start DAB audio output.
     *
     * @dname    playDabService
     * @param    serviceId    Selected service identifier.
     * @param    componentId  Audio component within the service.
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate delegates to driver_.startDabService().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> playDabService(
        std::uint32_t serviceId, std::uint32_t componentId) override;

    /**
     * @brief    setVolume — set Si4684 output attenuation.
     *
     * @dname    setVolume
     * @param    level  Attenuator 0–63.
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate writes volume_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> setVolume(
        std::uint8_t level) override;

private:
    /**
     * @brief    mapError — translate Si4684Error to core::TunerError.
     *
     * @dname    mapError
     * @param    error  Driver-level failure cause.
     * @return   Equivalent TunerError for services and HTTP layer.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static core::TunerError mapError(Si4684Error error) noexcept;

    Si4684Driver& driver_;
    std::uint8_t dabIndex_;
    core::FrequencyKHz fmFrequency_;
    std::uint8_t volume_;
};

} // namespace si4684
