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

#include "core/DabDynamicLabelAccumulator.hpp"
#include "core/DlsDecoder.hpp"
#include "core/FrequencyKHz.hpp"
#include "core/ITuner.hpp"
#include "core/RdsMetadataAccumulator.hpp"
#include "si4684/Si4684Driver.hpp"

#include <optional>

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
     * @param    antCap     Forwarded to Si4684Driver::tuneDab (0 = auto).
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate writes dabIndex_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneDab(
        std::uint8_t freqIndex, std::uint8_t antCap = 0U) override;

    /**
     * @brief    tuneFm — tune to an FM centre frequency in kHz.
     *
     * @dname    tuneFm
     * @param    frequency  Validated FM centre frequency.
     * @param    antCap     Forwarded to Si4684Driver::tuneFm (0 = auto).
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate writes fmFrequency_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneFm(
        core::FrequencyKHz frequency, std::uint8_t antCap = 0U) override;

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
     * @brief    readSignal — RSQ (FM) or DIGRAD (DAB) only.
     *
     * @dname    readSignal
     * @return   TunerStatus without RDS / DLS fields, or TunerError.
     * @pubstate sends FM_RSQ_STATUS or DAB_DIGRAD_STATUS only.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::expected<core::TunerStatus, core::TunerError>
    readSignal() override;

    /**
     * @brief    readDabLinkage — FIG 0/6, 0/24 and 0/21 for a service.
     *
     * @dname    readDabLinkage
     * @param    serviceId  Service playing in the current ensemble.
     * @return   Parsed linkage; parts the chip has not decoded yet are
     *           left empty. WrongBand outside DAB.
     * @pubstate sends AN649 0xB7, 0xC1 and 0xBF.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::expected<core::DabLinkage, core::TunerError>
    readDabLinkage(std::uint32_t serviceId) override;

    /**
     * @brief    pollBroadcastData — drain GET_DIGITAL_SERVICE_DATA (DLS, MOT).
     *
     * @dname    pollBroadcastData
     * @param    sink        Receiver of X-PAD MOT data groups.
     * @param    maxPackets  Packets read at most in this call.
     * @return   Ok (also outside DAB or with no service playing), or the
     *           mapped driver error.
     * @pubstate reads/acknowledges the chip's DSRV queue until BUFF_COUNT
     *           is 0; updates dls_ for DLS blocks.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::expected<void, core::TunerError> pollBroadcastData(
        core::IDabDataSink& sink, std::size_t maxPackets) override;

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

    /**
     * @brief    recalibrateXtal — re-run driver boot() with new crystal
     *           parameters, without an ESP32 restart.
     *
     * @dname    recalibrateXtal
     * @param    ibias      New POWER_UP ARG3 IBIAS.
     * @param    ctun       New POWER_UP ARG8 CTUN.
     * @param    xtalFreqHz New POWER_UP ARG4-7 XTAL_FREQ in Hz.
     * @return   Ok on success, or a mapped TunerError.
     * @pubstate delegates to driver_.recalibrateXtal(); caller must re-tune
     *           afterwards, this only reboots the chip.
     *
     * @author   Michele Bigi
     * @date     2026-08-23
     */
    [[nodiscard]] std::expected<void, core::TunerError> recalibrateXtal(
        std::uint8_t ibias, std::uint8_t ctun, std::uint32_t xtalFreqHz);

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

    /**
     * @brief    ensureBandLoaded — HOST_LOAD FM/DAB image when band differs.
     *
     * @dname    ensureBandLoaded
     * @param    band  Required application band for the next operation.
     * @return   Ok when the chip runs the requested image.
     * @pubstate may reload firmware (~1 s) and stop an active DAB service.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<void, core::TunerError> ensureBandLoaded(
        core::TunerBand band);

    Si4684Driver& driver_;
    std::uint8_t dabIndex_;
    core::FrequencyKHz fmFrequency_;
    std::uint8_t volume_;

    [[nodiscard]] std::expected<void, core::TunerError> readSignalInto(
        core::TunerStatus& status);

    /**
     * @brief    applyDlsSegment — feed one DLS DSRV block to the decoder.
     *
     * @dname    applyDlsSegment
     * @param    data  GET_DIGITAL_SERVICE_DATA block with DATA_SRC = DLS.
     * @pubstate writes dabDynamicLabel_ (segment join) and dls_ (decode
     *           once the payload is complete).
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void applyDlsSegment(const Si4684DabServiceData& data);

    /**
     * @brief    dispatchDabData — route one DSRV block (DLS or MOT).
     *
     * @dname    dispatchDabData
     * @param    data  Block from GET_DIGITAL_SERVICE_DATA.
     * @param    sink  Receiver of MOT data groups.
     * @pubstate DLS updates dls_ (applyDlsSegment); MOT goes to sink;
     *           other data services are dropped.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void dispatchDabData(const Si4684DabServiceData& data,
                         core::IDabDataSink& sink);

    /**
     * @brief    drainRds — read queued RDS groups into rdsMetadata_.
     *
     * @dname    drainRds
     * @param    maxGroups  Groups read at most.
     * @return   Ok when the FIFO is empty or the bound is reached; the
     *           mapped driver error otherwise.
     * @pubstate writes rdsMetadata_ (PS, RT, RT+, AF, PI).
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::expected<void, core::TunerError> drainRds(
        std::size_t maxGroups);
    bool fmBandReady_;
    core::RdsMetadataAccumulator rdsMetadata_;
    core::DabDynamicLabelAccumulator dabDynamicLabel_;
    core::DlsDecoder dls_;
    std::optional<std::uint32_t> lastDabServiceId_;
    std::optional<std::uint32_t> lastDabComponentId_;
};

} // namespace si4684
