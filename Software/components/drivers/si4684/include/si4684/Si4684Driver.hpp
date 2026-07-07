/**
 * @file    Si4684Driver.hpp
 * @brief   Si4684 DAB+/FM tuner — boot, tuning, status, and service control.
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
#include "core/IFirmwareBlobReader.hpp"
#include "core/SeekDirection.hpp"
#include "si4684/Si4684Band.hpp"
#include "si4684/Si4684Error.hpp"
#include "si4684/Si4684Types.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace si4684 {

/**
 * @brief    Si4684Pins — board GPIO/SPI identifiers for the tuner.
 *
 * @dname    Si4684Pins
 * @return   n/a (type)
 * @pubstate Immutable wiring snapshot from board_pins.hpp.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684Pins {
    int spiHost;  ///< ESP32 SPI host peripheral index.
    int csGpio;   ///< Chip-select GPIO.
    int misoGpio; ///< MISO GPIO.
    int mosiGpio; ///< MOSI GPIO.
    int sclkGpio; ///< SCLK GPIO.
    int rstbGpio; ///< RESET# GPIO (active low).
    int intbGpio; ///< INTB GPIO (optional status interrupt).
};

/**
 * @brief    Si4684Driver — full Si4684 control over SPI (AN649 / PE5PVB).
 *
 * @dname    Si4684Driver
 * @return   n/a (type)
 * @pubstate Owns the SPI link and boot state. Register-level command bytes
 *           stay private; callers use intent-level methods only.
 *
 * Implements the documented boot sequence plus band-specific tuning, property
 * access, RSQ/DIGRAD reads, and DAB service selection.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Si4684Driver {
public:
    /**
     * @brief    Si4684Driver — construct with board pins and firmware blobs.
     *
     * @dname    Si4684Driver
     * @param    pins      Board SPI and GPIO wiring.
     * @param    patch     ROM patch blob reader for HOST_LOAD.
     * @param    dabImage  DAB application image reader.
     * @param    fmImage   FM application image reader.
     * @pubstate stores pin map and blob references; not booted until boot().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    Si4684Driver(Si4684Pins pins,
                 const core::IFirmwareBlobReader& patch,
                 const core::IFirmwareBlobReader& dabImage,
                 const core::IFirmwareBlobReader& fmImage);

    /**
     * @brief    ~Si4684Driver — release SPI resources.
     *
     * @dname    ~Si4684Driver
     * @pubstate removes SPI device and bus when active.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~Si4684Driver();

    Si4684Driver(const Si4684Driver&) = delete;
    Si4684Driver& operator=(const Si4684Driver&) = delete;

    /**
     * @brief    boot — cold-start: pulse RSTB#, load patch/image, configure I/O.
     *
     * @details  Pulses RSTB# (active-low reset): gpio_config enables the ESP32
     *           internal pull-down while switching GPIO38 to output so the net
     *           cannot glitch high before gpio_set_level(0). That guards the
     *           Si4684 datasheet rule that RSTB must stay low until supplies are
     *           stable; the board's external pull-down already holds RSTB during
     *           rail ramp before app_main runs.
     *
     * @dname    boot
     * @param    band  DAB or FM application to load.
     * @return   Ok on success, or Si4684Error.
     * @pubstate writes booted_ and loadedBand_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> boot(Si4684Band band);

    /**
     * @brief    isBooted — query whether boot completed successfully.
     *
     * @dname    isBooted
     * @return   true after a successful boot().
     * @pubstate reads booted_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] bool isBooted() const noexcept;

    /**
     * @brief    loadedBand — read the active application band.
     *
     * @dname    loadedBand
     * @return   Si4684Band loaded by the last successful boot().
     * @pubstate reads loadedBand_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] Si4684Band loadedBand() const noexcept;

    /**
     * @brief    getPartInfo — read chip identity and firmware revision.
     *
     * @dname    getPartInfo
     * @return   Si4684PartInfo on success, or Si4684Error.
     * @pubstate reads chip via GET_PART_INFO (AN649).
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<Si4684PartInfo, Si4684Error> getPartInfo();

    /**
     * @brief    getSysState — read the running application state.
     *
     * @dname    getSysState
     * @return   Si4684SysState on success, or Si4684Error.
     * @pubstate reads chip via GET_SYS_STATE.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<Si4684SysState, Si4684Error> getSysState();

    /**
     * @brief    setProperty — write a Skyworks property (SET_PROPERTY 0x13).
     *
     * @dname    setProperty
     * @param    propertyId  16-bit property address.
     * @param    value       16-bit property value.
     * @return   Ok on success, or Si4684Error.
     * @pubstate writes property via SPI command.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> setProperty(
        std::uint16_t propertyId, std::uint16_t value);

    /**
     * @brief    setVolume — set audio attenuator 0–63 (property 0x0300).
     *
     * @dname    setVolume
     * @param    level  Attenuator level 0–63.
     * @return   Ok on success, or Si4684Error.
     * @pubstate writes AUDIO_VOLUME property.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> setVolume(std::uint8_t level);

    /**
     * @brief    tuneFm — tune FM to a validated centre frequency.
     *
     * @dname    tuneFm
     * @param    frequency  FM centre frequency in kHz.
     * @return   Ok on success, or Si4684Error::WrongBand / TuneFailed.
     * @pubstate sends FM_TUNE_FREQ and waits for STC (AN649).
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> tuneFm(
        core::FrequencyKHz frequency);

    /**
     * @brief    seekFm — seek FM in the given direction.
     *
     * @dname    seekFm
     * @param    direction  Up or Down scan direction.
     * @param    wrap       Band wrap behaviour for FM_SEEK_START.
     * @return   New centre frequency on success, or Si4684Error.
     * @pubstate sends FM_SEEK_START and waits for STC.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::FrequencyKHz, Si4684Error> seekFm(
        core::SeekDirection direction, SeekBandWrap wrap);

    /**
     * @brief    readFmRsq — read FM signal quality metrics.
     *
     * @dname    readFmRsq
     * @return   Si4684FmRsq on success, or Si4684Error.
     * @pubstate reads FM_RSQ_STATUS response bytes.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<Si4684FmRsq, Si4684Error> readFmRsq();

    /**
     * @brief    readFmRds — read a raw RDS group snapshot.
     *
     * @dname    readFmRds
     * @return   Si4684FmRdsStatus on success, or Si4684Error.
     * @pubstate reads FM_RDS_STATUS response bytes.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<Si4684FmRdsStatus, Si4684Error> readFmRds();

    /**
     * @brief    readDabServiceData — read one queued DAB data-service block.
     *
     * @dname    readDabServiceData
     * @param    statusOnly  Poll queue depth without consuming payload.
     * @param    ack         Acknowledge DSRVINT when reading payload.
     * @return   Data block on success, nullopt when queue empty, or Si4684Error.
     * @pubstate reads GET_DIGITAL_SERVICE_DATA response bytes.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<std::optional<Si4684DabServiceData>, Si4684Error>
    readDabServiceData(bool statusOnly, bool ack);

    /**
     * @brief    installDefaultDabFrequencyPlan — load Band III frequency list.
     *
     * @dname    installDefaultDabFrequencyPlan
     * @return   Ok on success, or Si4684Error.
     * @pubstate sends DAB_SET_FREQ_LIST with kDefaultDabFrequencyKhz.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> installDefaultDabFrequencyPlan();

    /**
     * @brief    tuneDab — tune to a Band III ensemble index.
     *
     * @dname    tuneDab
     * @param    freqIndex  Ensemble index 0–37.
     * @return   Ok on success, or Si4684Error.
     * @pubstate sends DAB_TUNE_FREQ and waits for STC.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> tuneDab(std::uint8_t freqIndex);

    /**
     * @brief    readDabDigRadStatus — read ensemble lock metrics.
     *
     * @dname    readDabDigRadStatus
     * @return   Si4684DabDigRadStatus on success, or Si4684Error.
     * @pubstate reads DAB_DIGRAD_STATUS response bytes.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<Si4684DabDigRadStatus, Si4684Error>
    readDabDigRadStatus();

    /**
     * @brief    readDabEventStatus — read DAB event flags.
     *
     * @dname    readDabEventStatus
     * @return   Si4684DabEventStatus on success, or Si4684Error.
     * @pubstate reads DAB_GET_EVENT_STATUS response bytes.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<Si4684DabEventStatus, Si4684Error>
    readDabEventStatus();

    /**
     * @brief    fetchDabServiceList — retrieve programmes for the ensemble.
     *
     * @dname    fetchDabServiceList
     * @return   Service rows on success, or Si4684Error.
     * @pubstate reads GET_DIGITAL_SERVICE_LIST chunks from the chip.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<std::vector<Si4684DabService>, Si4684Error>
    fetchDabServiceList();

    /**
     * @brief    startDabService — start DAB audio for a programme.
     *
     * @dname    startDabService
     * @param    serviceId    Selected service identifier.
     * @param    componentId  Audio component within the service.
     * @param    type         Digital service type (audio by default).
     * @return   Ok on success, or Si4684Error.
     * @pubstate sends START_DIGITAL_SERVICE.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Si4684Error> startDabService(
        std::uint32_t serviceId,
        std::uint32_t componentId,
        Si4684DigitalServiceType type = Si4684DigitalServiceType::Audio);

    /**
     * @brief    stopDabService — stop active DAB audio (STOP_DIGITAL_SERVICE).
     *
     * @dname    stopDabService
     * @param    serviceId    Selected service identifier.
     * @param    componentId  Audio component within the service.
     * @param    type         Digital service type (audio by default).
     * @return   Ok on success, or Si4684Error.
     * @pubstate sends STOP_DIGITAL_SERVICE (AN649 opcode 0x82).
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, Si4684Error> stopDabService(
        std::uint32_t serviceId,
        std::uint32_t componentId,
        Si4684DigitalServiceType type = Si4684DigitalServiceType::Audio);

private:
    enum class Command : std::uint8_t {
        PowerUp = 0x01,
        HostLoad = 0x04,
        LoadInit = 0x06,
        BootCmd = 0x07,
        GetPartInfo = 0x08,
        GetSysState = 0x09,
        GetFuncInfo = 0x12,
        SetProperty = 0x13,
        FmTuneFreq = 0x30,
        FmSeekStart = 0x31,
        FmRsqStatus = 0x32,
        FmRdsStatus = 0x34,
        GetDigitalServiceList = 0x80,
        StartDigitalService = 0x81,
        StopDigitalService = 0x82,
        GetDigitalServiceData = 0x84,
        DabTuneFreq = 0xB0,
        DabDigRadStatus = 0xB2,
        DabGetEventStatus = 0xB3,
        DabSetFreqList = 0xB8,
    };

    [[nodiscard]] std::expected<void, Si4684Error> ensureBooted() const;
    [[nodiscard]] std::expected<void, Si4684Error> ensureBand(
        Si4684Band band) const;
    [[nodiscard]] std::expected<void, Si4684Error> waitCts();
    [[nodiscard]] std::expected<void, Si4684Error> waitStc();
    [[nodiscard]] std::expected<void, Si4684Error> sendCommand(
        std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::expected<void, Si4684Error> readRaw(
        std::span<std::uint8_t> buffer);
    [[nodiscard]] std::expected<void, Si4684Error> writeCommand(
        Command cmd, const std::uint8_t* payload, std::size_t length);
    [[nodiscard]] std::expected<void, Si4684Error> hostLoadBlob(
        const core::IFirmwareBlobReader& blob, std::size_t chunkPayload);
    [[nodiscard]] std::expected<void, Si4684Error> configureAfterBoot(
        Si4684Band band);

    Si4684Pins pins_;
    const core::IFirmwareBlobReader& patch_;
    const core::IFirmwareBlobReader& dabImage_;
    const core::IFirmwareBlobReader& fmImage_;
    bool booted_;
    Si4684Band loadedBand_;
    bool spiBusActive_;
    void* spiDevice_;
};

} // namespace si4684
