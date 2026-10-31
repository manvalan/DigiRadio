/**
 * @file    Adau1452Eeprom.cpp
 * @brief   Bit-banged 25AA1024 access through the DSP writer program.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "adau1452/Adau1452Eeprom.hpp"

#include "adau1452/Adau1452Images.hpp"

#include "core/BlobStore.hpp"
#include "core/DspProgramBlob.hpp"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace adau1452 {

namespace writer {
#if __has_include("adau1452_eeprom_writer_cells.inc")
#include "adau1452_eeprom_writer_cells.inc"
inline constexpr bool kAvailable = true;
#else
inline constexpr bool kAvailable = false;
inline constexpr std::uint16_t kCellSs = 0U;
inline constexpr std::uint16_t kCellMosi = 0U;
inline constexpr std::uint16_t kCellSclk = 0U;
inline constexpr std::uint16_t kCellMiso = 0U;
#endif
} // namespace writer

namespace {

constexpr char kTag[] = "Adau1452Eeprom";
constexpr char kNvsNamespace[] = "dsp_v2";
constexpr char kNvsKey[] = "eeprom_crc";

// 25AA1024 opcodes and page size (Microchip DS21836).
constexpr std::uint8_t kOpWrite = 0x02U;
constexpr std::uint8_t kOpRead = 0x03U;
constexpr std::uint8_t kOpReadStatus = 0x05U;
constexpr std::uint8_t kOpWriteEnable = 0x06U;
constexpr std::uint8_t kStatusBusy = 0x01U;
constexpr std::size_t kPageBytes = 256U;
constexpr int kMaxBusyPolls = 100; // tWC is 6 ms; each poll waits 1 tick

/** The cells are 8.24: high is 1.0. */
constexpr std::int32_t kCellHigh = 0x01000000;

/** Pins change once per sample: 21 us at 48 kHz, so a little more than one. */
constexpr std::uint32_t kPinSettleUs = 25U;

/** One SPI mode 0 master over four DSP cells; false after any bus error. */
class CellSpi {
public:
    explicit CellSpi(Adau1452Driver& driver) : driver_(driver) {}

    [[nodiscard]] bool ok() const noexcept { return ok_; }

    void idle()
    {
        pin(writer::kCellSs, 1);
        pin(writer::kCellSclk, 0);
    }

    void select() { pin(writer::kCellSs, 0); }
    void deselect() { pin(writer::kCellSs, 1); }

    [[nodiscard]] std::uint8_t transfer(std::uint8_t out)
    {
        std::uint8_t in = 0U;
        for (int bit = 7; bit >= 0; --bit) {
            pin(writer::kCellMosi, (out >> bit) & 1);
            pin(writer::kCellSclk, 1);
            in = static_cast<std::uint8_t>((in << 1) | readMiso());
            pin(writer::kCellSclk, 0);
        }
        return in;
    }

private:
    void pin(std::uint16_t cell, int level)
    {
        ok_ = ok_ && driver_.writeCell(cell, level != 0 ? kCellHigh : 0).has_value();
        esp_rom_delay_us(kPinSettleUs);
    }

    [[nodiscard]] std::uint8_t readMiso()
    {
        const auto level = driver_.readCell(writer::kCellMiso);
        ok_ = ok_ && level.has_value();
        return level && *level != 0 ? 1U : 0U;
    }

    Adau1452Driver& driver_;
    bool ok_ = true;
};

void sendAddress(CellSpi& spi, std::uint32_t address)
{
    (void)spi.transfer(static_cast<std::uint8_t>(address >> 16));
    (void)spi.transfer(static_cast<std::uint8_t>(address >> 8));
    (void)spi.transfer(static_cast<std::uint8_t>(address));
}

[[nodiscard]] bool waitWhileBusy(CellSpi& spi)
{
    for (int poll = 0; poll < kMaxBusyPolls && spi.ok(); ++poll) {
        spi.select();
        (void)spi.transfer(kOpReadStatus);
        const std::uint8_t status = spi.transfer(0U);
        spi.deselect();
        if ((status & kStatusBusy) == 0U) {
            return true;
        }
        vTaskDelay(1);
    }
    return false;
}

[[nodiscard]] bool writePage(CellSpi& spi, std::uint32_t address,
                             std::span<const std::uint8_t> page)
{
    spi.select();
    (void)spi.transfer(kOpWriteEnable);
    spi.deselect();
    spi.select();
    (void)spi.transfer(kOpWrite);
    sendAddress(spi, address);
    for (const std::uint8_t byte : page) {
        (void)spi.transfer(byte);
    }
    spi.deselect();
    return waitWhileBusy(spi);
}

[[nodiscard]] bool writeImage(CellSpi& spi, std::span<const std::uint8_t> image)
{
    for (std::size_t at = 0U; at < image.size(); at += kPageBytes) {
        const std::size_t size = std::min(kPageBytes, image.size() - at);
        if (!writePage(spi, static_cast<std::uint32_t>(at),
                       image.subspan(at, size))) {
            ESP_LOGE(kTag, "page write failed at 0x%05X",
                     static_cast<unsigned>(at));
            return false;
        }
        if (at % 0x4000U == 0U) {
            ESP_LOGI(kTag, "written %u of %u bytes", static_cast<unsigned>(at),
                     static_cast<unsigned>(image.size()));
        }
    }
    return true;
}

[[nodiscard]] bool imageReadsBack(CellSpi& spi,
                                  std::span<const std::uint8_t> image)
{
    spi.select();
    (void)spi.transfer(kOpRead);
    sendAddress(spi, 0U);
    bool same = true;
    for (std::size_t at = 0U; at < image.size() && same && spi.ok(); ++at) {
        same = spi.transfer(0U) == image[at];
        if (!same) {
            ESP_LOGE(kTag, "read-back differs at 0x%05X",
                     static_cast<unsigned>(at));
        }
    }
    spi.deselect();
    return same && spi.ok();
}

[[nodiscard]] std::uint32_t crcOf(std::span<const std::uint8_t> image) noexcept
{
    return core::blobCrc32(std::as_bytes(image), 0U);
}

[[nodiscard]] std::optional<std::uint32_t> storedCrc()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return std::nullopt;
    }
    std::uint32_t crc = 0U;
    const esp_err_t err = nvs_get_u32(handle, kNvsKey, &crc);
    nvs_close(handle);
    return err == ESP_OK ? std::optional(crc) : std::nullopt;
}

void storeCrc(std::uint32_t crc)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(kTag, "EEPROM image not remembered: NVS open failed");
        return;
    }
    if (nvs_set_u32(handle, kNvsKey, crc) != ESP_OK
        || nvs_commit(handle) != ESP_OK) {
        ESP_LOGW(kTag, "EEPROM image not remembered: NVS write failed");
    }
    nvs_close(handle);
}

[[nodiscard]] bool startWriter(Adau1452Driver& driver,
                               std::span<const std::uint8_t> blob)
{
    const auto program = core::parseDspProgramBlob(blob);
    if (!program) {
        ESP_LOGE(kTag, "writer program unreadable");
        return false;
    }
    return driver.resetIntoSpiMode().has_value()
           && driver.download(*program).has_value();
}

} // namespace

EepromSync syncSelfBootEeprom(Adau1452Driver& driver)
{
    const auto image = selfBootImage();
    if (image.empty()) {
        return EepromSync::NoImage;
    }
    const std::uint32_t crc = crcOf(image);
    if (driver.selfBooted() && storedCrc() == crc) {
        return EepromSync::UpToDate;
    }
    const auto writerBlob = eepromWriterBlob();
    if (writerBlob.empty() || !writer::kAvailable) {
        ESP_LOGW(kTag, "EEPROM needs writing but this build has no writer");
        return EepromSync::NoWriter;
    }
    if (!startWriter(driver, writerBlob)) {
        return EepromSync::Failed;
    }
    ESP_LOGW(kTag, "writing %u bytes to the 25AA1024: keep the power on",
             static_cast<unsigned>(image.size()));
    CellSpi spi(driver);
    spi.idle();
    if (!writeImage(spi, image) || !imageReadsBack(spi, image)) {
        return EepromSync::Failed;
    }
    storeCrc(crc);
    ESP_LOGI(kTag, "self boot EEPROM written and verified");
    return EepromSync::Written;
}

} // namespace adau1452
