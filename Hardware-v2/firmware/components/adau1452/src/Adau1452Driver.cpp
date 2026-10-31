/**
 * @file    Adau1452Driver.cpp
 * @brief   Adau1452Driver implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "adau1452/Adau1452Driver.hpp"

#include "adau1452/Adau1452Cells.hpp"

#include "core/BiquadDesign.hpp"
#include "core/DspProgram.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace adau1452 {

namespace {

constexpr char kTag[] = "Adau1452";

constexpr std::uint16_t kRegPllEnable = 0xF003U;
constexpr std::uint16_t kRegHibernate = 0xF400U;
constexpr std::uint16_t kRegStartCore = 0xF402U;
constexpr std::uint16_t kRegCoreStatus = 0xF405U;
constexpr std::uint16_t kRegPanicFlag = 0xF427U;
constexpr std::uint16_t kRegPanicCode = 0xF428U;
constexpr std::uint16_t kRegFirst = 0xF000U;

/** Table 62, fixed since SigmaStudio 3.14. */
constexpr std::uint16_t kSafeloadData = 0x6000U;
constexpr std::uint16_t kSafeloadAddress = 0x6005U;
constexpr std::uint16_t kSafeloadCount = 0x6006U;
constexpr std::size_t kSafeloadMaxWords = 5U;

/** Table 36: chip address 0, LSB is R/W. */
constexpr std::uint8_t kWrite = 0x00U;
constexpr std::uint8_t kRead = 0x01U;
constexpr std::size_t kHeaderBytes = 3U;
/** Stays under the bus limit and on a multiple of every word size. */
constexpr std::size_t kBlockBytes = 4080U;
constexpr int kSpiHz = 10 * 1000 * 1000;

constexpr int kResetLowMs = 10;
/** Table 27: maximum PLL lock time 10.666 ms. */
constexpr int kPllLockMs = 11;
constexpr int kSelfBootPollMs = 20;
/** Band 0 is the program's fixed high-pass, as on v1: never overwritten. */
constexpr std::uint8_t kFixedHighPassBand = 0U;
/** Three attempts of a full 25AA1024 at the 1 MHz the data sheet allows. */
constexpr int kSelfBootTimeoutMs = 4000;

/** Bytes per address step: registers 2; program and data memory 4 (the
 *  export has 706 program words in 2824 bytes). */
[[nodiscard]] std::size_t wordBytes(std::uint16_t address) noexcept
{
    return address >= kRegFirst ? 2U : 4U;
}

/** Program mux inputs. core::ActiveSource::Bluetooth is the ESP32 stream, as
 *  on v1; input 1, the BT1058's own audio, has no ActiveSource yet. */
[[nodiscard]] std::int32_t muxIndex(core::ActiveSource source) noexcept
{
    switch (source) {
    case core::ActiveSource::Radio:
        return 0;
    case core::ActiveSource::Beep:
        return 2;
    case core::ActiveSource::Bluetooth:
    default:
        return 3;
    }
}

/** From identity to the compiled value as t goes 0 to 1, as v1 did. */
[[nodiscard]] std::int32_t blend(std::uint32_t compiled, float identity,
                                 float t) noexcept
{
    const float value =
        static_cast<float>(static_cast<std::int32_t>(compiled)) / 16777216.0F;
    return static_cast<std::int32_t>(
        std::lround((identity + (value - identity) * t) * 16777216.0F));
}

/** ADAU145x parameters are 8.24, not the 8.23 of the ADAU1701 helpers. */
[[nodiscard]] std::int32_t toFixpoint824(float value) noexcept
{
    constexpr double kOne = 16777216.0;
    const double scaled = static_cast<double>(value) * kOne;
    return static_cast<std::int32_t>(std::clamp(scaled, -2147483648.0,
                                                2147483647.0));
}

[[nodiscard]] std::int32_t gainToFixpoint(core::GainDb gain) noexcept
{
    return toFixpoint824(gain.linear());
}

[[nodiscard]] bool coreIsRunning(std::uint16_t status) noexcept
{
    // CORE_STATUS 1 running, 3 sleeping between samples, 4 stalled briefly.
    const unsigned state = status & 0x7U;
    return state == 1U || state == 3U || state == 4U;
}

class ScopedLock {
public:
    explicit ScopedLock(SemaphoreHandle_t lock) : lock_(lock)
    {
        xSemaphoreTake(lock_, portMAX_DELAY);
    }
    ~ScopedLock() { xSemaphoreGive(lock_); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    SemaphoreHandle_t lock_;
};

} // namespace

Adau1452Driver::Adau1452Driver(Adau1452Pins pins,
                               core::IDspProgramSource& programSource)
    : pins_(pins)
    , programSource_(programSource)
    , device_(nullptr)
    , lock_(nullptr)
    , booted_(false)
    , selfBooted_(false)
{
}

std::expected<void, Adau1452Error> Adau1452Driver::ensureDevice()
{
    if (lock_ == nullptr) {
        lock_ = xSemaphoreCreateMutex();
    }
    if (device_ != nullptr) {
        return {};
    }
    spi_device_interface_config_t cfg = {};
    cfg.clock_speed_hz = kSpiHz;
    cfg.mode = 3; // SPI Slave Port section: CPOL = CPHA = 1
    cfg.spics_io_num = pins_.csGpio;
    cfg.queue_size = 1;
    spi_device_handle_t dev = nullptr;
    if (lock_ == nullptr
        || spi_bus_add_device(static_cast<spi_host_device_t>(pins_.spiHost),
                              &cfg, &dev) != ESP_OK) {
        return std::unexpected(Adau1452Error::SpiFailed);
    }
    device_ = dev;
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::ensureReady() const
{
    if (!booted_) {
        return std::unexpected(Adau1452Error::NotBooted);
    }
    if (!cells::kAvailable) {
        return std::unexpected(Adau1452Error::CellsMissing);
    }
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::transfer(
    std::uint8_t rw, std::uint16_t address, std::span<const std::uint8_t> tx,
    std::span<std::uint8_t> rx)
{
    const std::size_t total = kHeaderBytes + std::max(tx.size(), rx.size());
    auto* buffer = static_cast<std::uint8_t*>(
        heap_caps_calloc(total, 1U, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (buffer == nullptr) {
        return std::unexpected(Adau1452Error::SpiFailed);
    }
    buffer[0] = rw;
    buffer[1] = static_cast<std::uint8_t>(address >> 8);
    buffer[2] = static_cast<std::uint8_t>(address & 0xFFU);
    if (!tx.empty()) {
        std::memcpy(buffer + kHeaderBytes, tx.data(), tx.size());
    }
    spi_transaction_t txn = {};
    txn.length = total * 8U;
    txn.tx_buffer = buffer;
    txn.rx_buffer = rx.empty() ? nullptr : buffer;
    const esp_err_t err = spi_device_transmit(
        static_cast<spi_device_handle_t>(device_), &txn);
    if (err == ESP_OK && !rx.empty()) {
        std::memcpy(rx.data(), buffer + kHeaderBytes, rx.size());
    }
    heap_caps_free(buffer);
    if (err != ESP_OK) {
        return std::unexpected(Adau1452Error::SpiFailed);
    }
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::writeBlock(
    std::uint16_t address, std::span<const std::uint8_t> data)
{
    const std::size_t step = wordBytes(address);
    std::size_t done = 0U;
    while (done < data.size()) {
        const std::size_t chunk = std::min(kBlockBytes, data.size() - done);
        const auto at = static_cast<std::uint16_t>(address + done / step);
        if (auto sent = transfer(kWrite, at, data.subspan(done, chunk), {});
            !sent) {
            return sent;
        }
        done += chunk;
    }
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::writeRegister(
    std::uint16_t address, std::uint16_t value)
{
    const std::array<std::uint8_t, 2> data = {
        static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value & 0xFFU),
    };
    return transfer(kWrite, address, data, {});
}

std::expected<std::uint16_t, Adau1452Error> Adau1452Driver::readRegister(
    std::uint16_t address)
{
    std::array<std::uint8_t, 2> data = {};
    if (auto got = transfer(kRead, address, {}, data); !got) {
        return std::unexpected(got.error());
    }
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

std::expected<void, Adau1452Error> Adau1452Driver::writeCell(
    std::uint16_t address, std::int32_t value)
{
    const auto raw = static_cast<std::uint32_t>(value);
    const std::array<std::uint8_t, 4> data = {
        static_cast<std::uint8_t>(raw >> 24),
        static_cast<std::uint8_t>(raw >> 16),
        static_cast<std::uint8_t>(raw >> 8),
        static_cast<std::uint8_t>(raw),
    };
    return transfer(kWrite, address, data, {});
}

std::expected<std::int32_t, Adau1452Error> Adau1452Driver::readCell(
    std::uint16_t address)
{
    std::array<std::uint8_t, 4> data = {};
    if (auto got = transfer(kRead, address, {}, data); !got) {
        return std::unexpected(got.error());
    }
    return static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(data[0]) << 24)
        | (static_cast<std::uint32_t>(data[1]) << 16)
        | (static_cast<std::uint32_t>(data[2]) << 8) | data[3]);
}

std::expected<void, Adau1452Error> Adau1452Driver::resetIntoSpiMode()
{
    booted_ = false;
    selfBooted_ = false;
    if (auto dev = ensureDevice(); !dev) {
        return dev;
    }
    const auto reset = static_cast<gpio_num_t>(pins_.resetGpio);
    gpio_set_level(reset, 0);
    if (gpio_set_direction(reset, GPIO_MODE_OUTPUT) != ESP_OK) {
        return std::unexpected(Adau1452Error::SpiFailed);
    }
    vTaskDelay(pdMS_TO_TICKS(kResetLowMs));
    gpio_set_level(reset, 1);
    vTaskDelay(pdMS_TO_TICKS(kPllLockMs));

    // SPI Slave Port section: three dummy writes count as the three SS
    // toggles and are not acknowledged. The port stays in SPI until reset.
    for (int i = 0; i < 3; ++i) {
        if (auto dummy = writeRegister(kRegFirst, 0U); !dummy) {
            return dummy;
        }
    }
    return {};
}

std::expected<bool, Adau1452Error> Adau1452Driver::waitForSelfBoot()
{
    for (int waited = 0; waited < kSelfBootTimeoutMs;
         waited += kSelfBootPollMs) {
        const auto status = readRegister(kRegCoreStatus);
        if (!status) {
            return std::unexpected(status.error());
        }
        if (coreIsRunning(*status)) {
            return true;
        }
        const auto panic = readRegister(kRegPanicFlag);
        if (!panic) {
            return std::unexpected(panic.error());
        }
        if (*panic != 0U) {
            const auto code = readRegister(kRegPanicCode);
            ESP_LOGW(kTag, "self boot failed, panic code 0x%04X",
                     code ? *code : 0xFFFFU);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(kSelfBootPollMs));
    }
    ESP_LOGW(kTag, "no self boot within %d ms", kSelfBootTimeoutMs);
    return false;
}

std::expected<void, Adau1452Error> Adau1452Driver::download(
    const core::DspProgram& program)
{
    booted_ = false;
    for (const core::RegisterWrite& write : program.writes()) {
        if (auto sent = writeBlock(write.address(), write.data()); !sent) {
            return std::unexpected(Adau1452Error::DownloadFailed);
        }
        // Waits of Table 27, which the export has as separate delay steps.
        if (write.address() == kRegPllEnable) {
            vTaskDelay(pdMS_TO_TICKS(kPllLockMs));
        } else if (write.address() == kRegHibernate
                   || write.address() == kRegStartCore) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    const auto status = readRegister(kRegCoreStatus);
    if (!status || !coreIsRunning(*status)) {
        ESP_LOGE(kTag, "core not running after download");
        return std::unexpected(Adau1452Error::DownloadFailed);
    }
    booted_ = true;
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::boot()
{
    if (auto spi = resetIntoSpiMode(); !spi) {
        return spi;
    }
    const auto selfBoot = waitForSelfBoot();
    if (!selfBoot) {
        return std::unexpected(selfBoot.error());
    }
    if (*selfBoot) {
        booted_ = true;
        selfBooted_ = true;
        ESP_LOGI(kTag, "running from the self boot EEPROM");
        return {};
    }
    auto program = programSource_.loadProgram();
    if (!program) {
        ESP_LOGE(kTag, "no self boot and no program to download");
        return std::unexpected(Adau1452Error::ProgramMissing);
    }
    ESP_LOGW(kTag, "no self boot: downloading the program over SPI");
    return download(*program);
}

std::expected<void, Adau1452Error> Adau1452Driver::safeload(
    std::uint16_t target, std::span<const std::int32_t> words)
{
    if (words.empty() || words.size() > kSafeloadMaxWords) {
        return std::unexpected(Adau1452Error::InvalidParameter);
    }
    ScopedLock guard(lock_);
    for (std::size_t i = 0U; i < words.size(); ++i) {
        const auto slot = static_cast<std::uint16_t>(kSafeloadData + i);
        if (auto sent = writeCell(slot, words[i]); !sent) {
            return sent;
        }
    }
    if (auto sent = writeCell(kSafeloadAddress, target); !sent) {
        return sent;
    }
    if (auto sent = writeCell(kSafeloadCount,
                              static_cast<std::int32_t>(words.size()));
        !sent) {
        return sent;
    }
    // Step 5 of the procedure: one frame, 21 us at 48 kHz; a tick covers it
    // and keeps the next caller from writing during the transfer.
    vTaskDelay(1);
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::selectSource(
    core::ActiveSource source)
{
    if (auto ready = ensureReady(); !ready) {
        return ready;
    }
    // SIGMASTUDIOTYPE_INTEGER in the export: a plain index, not 8.24.
    const std::array<std::int32_t, 1> index = {muxIndex(source)};
    return safeload(cells::kSourceSelect, index);
}

std::expected<void, Adau1452Error> Adau1452Driver::setMasterVolume(
    core::GainDb left, core::GainDb right)
{
    if (auto ready = ensureReady(); !ready) {
        return ready;
    }
    const std::array<std::int32_t, 1> l = {gainToFixpoint(left)};
    if (auto sent = safeload(cells::kMasterLeft, l); !sent) {
        return sent;
    }
    const std::array<std::int32_t, 1> r = {gainToFixpoint(right)};
    return safeload(cells::kMasterRight, r);
}

std::expected<void, Adau1452Error> Adau1452Driver::setEqBand(
    core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center, float q)
{
    if (band.value() == kFixedHighPassBand) {
        return std::unexpected(Adau1452Error::InvalidParameter);
    }
    if (auto ready = ensureReady(); !ready) {
        return ready;
    }
    const core::BiquadCoefficients c = core::designPeakingEq(center, gain, q);
    // Address order of the export: b2 b1 b0 a2 a1. BiquadCoefficients calls
    // the feedback terms a0 (the export's A1) and a1 (its A2), same sign.
    const std::array<std::int32_t, 5> words = {
        toFixpoint824(c.b2), toFixpoint824(c.b1), toFixpoint824(c.b0),
        toFixpoint824(c.a1), toFixpoint824(c.a0),
    };
    return safeload(cells::kEqBand[band.value()], words);
}

std::expected<void, Adau1452Error> Adau1452Driver::applyEq(
    const core::EqProfile& eq)
{
    for (std::uint8_t i = kFixedHighPassBand + 1U;
         i < core::EqBandIndex::kBandCount; ++i) {
        const auto index = core::EqBandIndex::tryFromIndex(i);
        if (!index) {
            return std::unexpected(Adau1452Error::InvalidParameter);
        }
        const core::EqBandSettings& band = eq.band(*index);
        if (auto sent = setEqBand(*index, band.gain, band.center, band.q);
            !sent) {
            return sent;
        }
    }
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::setBeepEnabled(bool enabled)
{
    if (auto ready = ensureReady(); !ready) {
        return ready;
    }
    const std::array<std::int32_t, 1> value = {
        enabled ? toFixpoint824(1.0F) : 0};
    return safeload(cells::kBeepEnable, value);
}

std::expected<void, Adau1452Error> Adau1452Driver::writeBassBlock(
    std::size_t block, float t)
{
    // Filter order b0 b2 b1 a2 a1: only b0 is 1 in the identity filter.
    std::array<std::int32_t, 5> filter = {};
    for (std::size_t i = 0U; i < filter.size(); ++i) {
        filter[i] = blend(cells::kBassFilterValues[i], i == 0U ? 1.0F : 0.0F, t);
    }
    if (auto sent = safeload(cells::kBassFilter[block], filter); !sent) {
        return sent;
    }
    std::array<std::int32_t, kSafeloadMaxWords> words = {};
    for (std::size_t at = 0U; at < cells::kBassTableValues.size();
         at += words.size()) {
        for (std::size_t i = 0U; i < words.size(); ++i) {
            words[i] = blend(cells::kBassTableValues[at + i], 1.0F, t);
        }
        const auto cell =
            static_cast<std::uint16_t>(cells::kBassTable[block] + at);
        if (auto sent = safeload(cell, words); !sent) {
            return sent;
        }
    }
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::setBassBoostLevel(
    core::EnhanceLevel level)
{
    if (auto ready = ensureReady(); !ready) {
        return ready;
    }
    for (std::size_t block = 0U; block < cells::kBassFilter.size(); ++block) {
        if (auto sent = writeBassBlock(block, level.fraction()); !sent) {
            return sent;
        }
    }
    return {};
}

std::expected<void, Adau1452Error> Adau1452Driver::setStereoSpreadLevel(
    core::EnhanceLevel level)
{
    if (auto ready = ensureReady(); !ready) {
        return ready;
    }
    const float t = level.fraction();
    const std::array<std::int32_t, 1> spread = {
        blend(cells::kSpreadLevelValue, 0.0F, t)};
    if (auto sent = safeload(cells::kSpreadLevel, spread); !sent) {
        return sent;
    }
    const std::array<std::int32_t, 1> alpha = {
        blend(cells::kAlphaSpreadValue, 0.0F, t)};
    return safeload(cells::kAlphaSpread, alpha);
}

std::expected<void, Adau1452Error> Adau1452Driver::writeRawParam(
    unsigned address, float value)
{
    if (!booted_) {
        return std::unexpected(Adau1452Error::NotBooted);
    }
    const std::array<std::int32_t, 1> word = {toFixpoint824(value)};
    return safeload(static_cast<std::uint16_t>(address), word);
}

std::expected<void, Adau1452Error> Adau1452Driver::applyProfile(
    const core::AudioProfile& profile)
{
    if (auto sent = selectSource(profile.activeSource); !sent) {
        return sent;
    }
    if (auto sent = applyEq(profile.eq); !sent) {
        return sent;
    }
    if (auto sent = setMasterVolume(profile.masterLeft, profile.masterRight);
        !sent) {
        return sent;
    }
    if (auto sent = setBassBoostLevel(profile.enhancements.bass); !sent) {
        return sent;
    }
    return setStereoSpreadLevel(profile.enhancements.stereo);
}

std::expected<core::AudioLevels, Adau1452Error> Adau1452Driver::readLevels()
{
    if (auto ready = ensureReady(); !ready) {
        return std::unexpected(ready.error());
    }
    std::array<float, 6> db = {};
    for (std::size_t i = 0U; i < db.size(); ++i) {
        const auto raw = readCell(cells::kLevel[i]);
        if (!raw) {
            return std::unexpected(raw.error());
        }
        constexpr float kFloorDb = -96.0F;
        const float linear =
            std::fabs(static_cast<float>(*raw) / 16777216.0F);
        db[i] = linear < 1e-5F ? kFloorDb
                               : std::max(kFloorDb, 20.0F * std::log10(linear));
    }
    return core::AudioLevels{db[0], db[1], db[2], db[3], db[4], db[5]};
}

} // namespace adau1452
