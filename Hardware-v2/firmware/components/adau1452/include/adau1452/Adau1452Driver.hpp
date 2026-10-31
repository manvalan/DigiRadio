/**
 * @file    Adau1452Driver.hpp
 * @brief   ADAU1452 over the SPI slave port: boot, safeload, cell access.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Register addresses and sequences: ADAU1452 data sheet Rev. D, sections
 * System Initialization Sequence (Table 27), SPI Slave Port, Self Boot and
 * Software Safeload (Table 62).
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include "core/ActiveSource.hpp"
#include "core/AudioLevels.hpp"
#include "core/AudioProfile.hpp"
#include "core/EqBandIndex.hpp"
#include "core/EqProfile.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"
#include "core/IDspProgramSource.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>
#include <expected>
#include <span>

namespace adau1452 {

/**
 * @brief    Adau1452Error — failure causes of the driver.
 *
 * @dname    Adau1452Error
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
enum class Adau1452Error {
    NotBooted,
    SpiFailed,
    ProgramMissing,
    DownloadFailed,
    CellsMissing,
    InvalidParameter,
};

/**
 * @brief    Adau1452Pins — board wiring of the slave port.
 *
 * @dname    Adau1452Pins
 * @return   n/a (type)
 * @pubstate Immutable wiring snapshot from board_pins.hpp.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
struct Adau1452Pins {
    int spiHost;   ///< Bus initialised elsewhere, shared with the Si4684.
    int csGpio;    ///< SS/ADDR0.
    int resetGpio; ///< RESET#, active low.
};

/**
 * @brief    Adau1452Driver — owns reset, the SPI device and parameter writes.
 *
 * @dname    Adau1452Driver
 * @param    pins           Board wiring.
 * @param    programSource  RAM download, used when self boot fails.
 * @return   n/a (type)
 * @pubstate booted_ after a self boot or a RAM download.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
class Adau1452Driver {
public:
    Adau1452Driver(Adau1452Pins pins, core::IDspProgramSource& programSource);
    Adau1452Driver(const Adau1452Driver&) = delete;
    Adau1452Driver& operator=(const Adau1452Driver&) = delete;

    /**
     * @brief    boot — reset, wait for self boot, fall back to a RAM download.
     *
     * @dname    boot
     * @return   Ok when a program runs, or Adau1452Error.
     * @pubstate leaves the slave port in SPI mode even on failure, so the
     *           Si4684 can use the bus: until then the ADAU1452 would read
     *           its traffic as I2C.
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<void, Adau1452Error> boot();

    /**
     * @brief    resetIntoSpiMode — pulse RESET# and enter SPI slave mode.
     *
     * @dname    resetIntoSpiMode
     * @return   Ok, or Adau1452Error.
     * @pubstate clears booted_. A self boot starts if the EEPROM holds one.
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<void, Adau1452Error> resetIntoSpiMode();

    /**
     * @brief    download — replay a program over the slave port.
     *
     * @dname    download
     * @param    program  Export of SigmaStudio+, starting at SOFT_RESET.
     * @return   Ok, or Adau1452Error.
     * @pubstate sets booted_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<void, Adau1452Error> download(
        const core::DspProgram& program);

    [[nodiscard]] bool isBooted() const noexcept { return booted_; }
    [[nodiscard]] bool selfBooted() const noexcept { return selfBooted_; }

    [[nodiscard]] std::expected<void, Adau1452Error> applyProfile(
        const core::AudioProfile& profile);
    [[nodiscard]] std::expected<void, Adau1452Error> selectSource(
        core::ActiveSource source);
    [[nodiscard]] std::expected<void, Adau1452Error> applyEq(
        const core::EqProfile& eq);
    [[nodiscard]] std::expected<void, Adau1452Error> setMasterVolume(
        core::GainDb left, core::GainDb right);
    [[nodiscard]] std::expected<void, Adau1452Error> setEqBand(
        core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center,
        float q);
    [[nodiscard]] std::expected<void, Adau1452Error> setBeepEnabled(
        bool enabled);
    [[nodiscard]] std::expected<void, Adau1452Error> setBassBoostLevel(
        core::EnhanceLevel level);
    [[nodiscard]] std::expected<void, Adau1452Error> setStereoSpreadLevel(
        core::EnhanceLevel level);
    [[nodiscard]] std::expected<void, Adau1452Error> writeRawParam(
        unsigned address, float value);
    [[nodiscard]] std::expected<core::AudioLevels, Adau1452Error> readLevels();

    /**
     * @brief    writeRegister — write one 16-bit control register.
     *
     * @dname    writeRegister
     * @param    address  Register address.
     * @param    value    Register value.
     * @return   Ok, or Adau1452Error.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<void, Adau1452Error> writeRegister(
        std::uint16_t address, std::uint16_t value);

    /**
     * @brief    readRegister — read one 16-bit control register.
     *
     * @dname    readRegister
     * @param    address  Register address.
     * @return   Value, or Adau1452Error.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<std::uint16_t, Adau1452Error> readRegister(
        std::uint16_t address);

    /**
     * @brief    writeCell — direct 32-bit write to data memory.
     *
     * @dname    writeCell
     * @param    address  Data memory address.
     * @param    value    Raw word.
     * @return   Ok, or Adau1452Error.
     * @pubstate none: applies at once, unlike a safeload.
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<void, Adau1452Error> writeCell(
        std::uint16_t address, std::int32_t value);

    /**
     * @brief    readCell — direct 32-bit read from data memory.
     *
     * @dname    readCell
     * @param    address  Data memory address.
     * @return   Raw word, or Adau1452Error.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-31
     */
    [[nodiscard]] std::expected<std::int32_t, Adau1452Error> readCell(
        std::uint16_t address);

private:
    [[nodiscard]] std::expected<void, Adau1452Error> ensureDevice();
    [[nodiscard]] std::expected<void, Adau1452Error> ensureReady() const;
    [[nodiscard]] std::expected<bool, Adau1452Error> waitForSelfBoot();
    [[nodiscard]] std::expected<void, Adau1452Error> transfer(
        std::uint8_t rw, std::uint16_t address, std::span<const std::uint8_t> tx,
        std::span<std::uint8_t> rx);
    [[nodiscard]] std::expected<void, Adau1452Error> writeBlock(
        std::uint16_t address, std::span<const std::uint8_t> data);
    [[nodiscard]] std::expected<void, Adau1452Error> safeload(
        std::uint16_t target, std::span<const std::int32_t> words);

    Adau1452Pins pins_;
    core::IDspProgramSource& programSource_;
    void* device_;
    SemaphoreHandle_t lock_;
    bool booted_;
    bool selfBooted_;
};

} // namespace adau1452
