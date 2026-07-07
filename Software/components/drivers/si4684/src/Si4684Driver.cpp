/**
 * @file    Si4684Driver.cpp
 * @brief   Si4684Driver implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "si4684/Si4684Driver.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

namespace si4684 {

namespace {

constexpr char kTag[] = "Si4684";
constexpr std::size_t kSpiBufferSize = 4096U;
constexpr int kCtsPollMs = 2;
constexpr int kCtsRetries = 200;
constexpr int kStcRetries = 250;
constexpr int kStcPollMs = 20;

constexpr std::uint16_t kPropDigitalIoOutputSelect = 0x0200U;
constexpr std::uint16_t kPropDigitalIoSampleRate = 0x0201U;
constexpr std::uint16_t kPropPinConfigEnable = 0x0800U;
constexpr std::uint16_t kPropAudioVolume = 0x0300U;
constexpr std::uint16_t kPropFmRdsConfig = 0x3C02U;
constexpr std::uint16_t kPropDabTuneFeCfg = 0x1712U;
constexpr std::uint16_t kPropDabXpadEnable = 0xB400U;
constexpr std::uint16_t kPropDigitalServiceIntSource = 0x8100U;

std::uint16_t readLe16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t readLe32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0])
           | (static_cast<std::uint32_t>(p[1]) << 8)
           | (static_cast<std::uint32_t>(p[2]) << 16)
           | (static_cast<std::uint32_t>(p[3]) << 24);
}

/** FM_TUNE_FREQ uses 10 kHz units (AN649); API uses kHz. */
[[nodiscard]] std::uint16_t kHzToChipFmFreq(std::uint32_t frequencyKhz)
{
    return static_cast<std::uint16_t>(frequencyKhz / 10U);
}

[[nodiscard]] std::uint32_t chipFmFreqToKHz(std::uint16_t chipFreq)
{
    return static_cast<std::uint32_t>(chipFreq) * 10U;
}

} // namespace

Si4684Driver::Si4684Driver(Si4684Pins pins,
                           const core::IFirmwareBlobReader& patch,
                           const core::IFirmwareBlobReader& dabImage,
                           const core::IFirmwareBlobReader& fmImage)
    : pins_(pins)
    , patch_(patch)
    , dabImage_(dabImage)
    , fmImage_(fmImage)
    , booted_(false)
    , loadedBand_(Si4684Band::Dab)
    , spiBusActive_(false)
    , spiDevice_(nullptr)
{
}

Si4684Driver::~Si4684Driver()
{
    if (spiDevice_ != nullptr) {
        spi_bus_remove_device(static_cast<spi_device_handle_t>(spiDevice_));
        spiDevice_ = nullptr;
    }
    if (spiBusActive_) {
        spi_bus_free(static_cast<spi_host_device_t>(pins_.spiHost));
        spiBusActive_ = false;
    }
}

std::expected<void, Si4684Error> Si4684Driver::ensureBooted() const
{
    if (!booted_) {
        return std::unexpected(Si4684Error::NotBooted);
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::ensureBand(
    Si4684Band band) const
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (loadedBand_ != band) {
        return std::unexpected(Si4684Error::WrongBand);
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::waitCts()
{
    std::array<std::uint8_t, 5> pollTx = {};
    std::array<std::uint8_t, 5> pollRx = {};

    for (int attempt = 0; attempt < kCtsRetries; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(kCtsPollMs));
        spi_transaction_t txn = {};
        txn.length = pollTx.size() * 8U;
        txn.tx_buffer = pollTx.data();
        txn.rx_buffer = pollRx.data();
        if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                                &txn) != ESP_OK) {
            return std::unexpected(Si4684Error::SpiInitFailed);
        }
        if ((pollRx[1] & 0x80U) != 0U) {
            return {};
        }
    }
    return std::unexpected(Si4684Error::CtsTimeout);
}

std::expected<void, Si4684Error> Si4684Driver::waitStc()
{
    std::array<std::uint8_t, 5> pollTx = {};
    std::array<std::uint8_t, 5> pollRx = {};

    for (int attempt = 0; attempt < kStcRetries; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(kStcPollMs));
        spi_transaction_t txn = {};
        txn.length = pollTx.size() * 8U;
        txn.tx_buffer = pollTx.data();
        txn.rx_buffer = pollRx.data();
        if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                                &txn) != ESP_OK) {
            return std::unexpected(Si4684Error::SpiInitFailed);
        }
        if ((pollRx[1] & 0x01U) != 0U) {
            return {};
        }
    }
    return std::unexpected(Si4684Error::StcTimeout);
}

std::expected<void, Si4684Error> Si4684Driver::sendCommand(
    std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() > kSpiBufferSize) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    spi_transaction_t txn = {};
    txn.length = bytes.size() * 8U;
    txn.tx_buffer = bytes.data();
    if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                            &txn) != ESP_OK) {
        return std::unexpected(Si4684Error::SpiInitFailed);
    }
    return waitCts();
}

std::expected<void, Si4684Error> Si4684Driver::readRaw(
    std::span<std::uint8_t> buffer)
{
    if (buffer.empty() || buffer.size() > kSpiBufferSize) {
        return std::unexpected(Si4684Error::ReplyTooShort);
    }
    std::fill(buffer.begin(), buffer.end(), 0U);
    spi_transaction_t txn = {};
    txn.length = buffer.size() * 8U;
    txn.tx_buffer = buffer.data();
    txn.rx_buffer = buffer.data();
    if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                            &txn) != ESP_OK) {
        return std::unexpected(Si4684Error::SpiInitFailed);
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::writeCommand(
    Command cmd, const std::uint8_t* payload, std::size_t length)
{
    if (length + 2U > kSpiBufferSize) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, kSpiBufferSize> buffer = {};
    buffer[0] = static_cast<std::uint8_t>(cmd);
    buffer[1] = 0x00U;
    if (payload != nullptr && length > 0U) {
        std::memcpy(buffer.data() + 2U, payload, length);
    }
    return sendCommand({buffer.data(), 2U + length});
}

std::expected<void, Si4684Error> Si4684Driver::hostLoadBlob(
    const core::IFirmwareBlobReader& blob, std::size_t chunkPayload)
{
    std::array<std::byte, 2044> payload = {};
    std::size_t offset = 0U;

    while (offset < blob.size()) {
        const std::size_t maxChunk = std::min(chunkPayload, payload.size());
        const std::size_t copied =
            blob.read(offset, std::span<std::byte>(payload.data(), maxChunk));
        if (copied == 0U) {
            return std::unexpected(Si4684Error::ImageLoadFailed);
        }

        std::array<std::uint8_t, kSpiBufferSize> tx = {};
        tx[0] = static_cast<std::uint8_t>(Command::HostLoad);
        tx[1] = 0x00U;
        tx[2] = 0x00U;
        tx[3] = 0x00U;
        std::memcpy(tx.data() + 4U, payload.data(), copied);

        spi_transaction_t txn = {};
        txn.length = (4U + copied) * 8U;
        txn.tx_buffer = tx.data();
        if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                                &txn) != ESP_OK) {
            return std::unexpected(Si4684Error::ImageLoadFailed);
        }
        if (auto cts = waitCts(); !cts) {
            return cts;
        }
        offset += copied;
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::setProperty(
    std::uint16_t propertyId, std::uint16_t value)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    const std::uint8_t args[] = {
        static_cast<std::uint8_t>(propertyId & 0xFFU),
        static_cast<std::uint8_t>(propertyId >> 8),
        static_cast<std::uint8_t>(value & 0xFFU),
        static_cast<std::uint8_t>(value >> 8),
    };
    if (auto cmd = writeCommand(Command::SetProperty, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::setVolume(std::uint8_t level)
{
    return setProperty(kPropAudioVolume,
                       static_cast<std::uint16_t>(level & 0x3FU));
}

std::expected<Si4684PartInfo, Si4684Error> Si4684Driver::getPartInfo()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }
    if (auto cmd = writeCommand(Command::GetPartInfo, nullptr, 0U); !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 24> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }
    if (auto fn = writeCommand(Command::GetFuncInfo, nullptr, 0U); !fn) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 13> fnRaw = {};
    if (auto rd = readRaw(fnRaw); !rd) {
        return std::unexpected(rd.error());
    }

    Si4684PartInfo info = {};
    info.chipId = readLe16(raw.data() + 9);
    info.firmwareMajor = fnRaw[5];
    info.firmwareMinor = fnRaw[6];
    info.firmwareBuild = fnRaw[7];
    return info;
}

std::expected<Si4684SysState, Si4684Error> Si4684Driver::getSysState()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }
    if (auto cmd = writeCommand(Command::GetSysState, nullptr, 0U); !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 7> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }
    Si4684SysState state = {};
    state.imageType = raw[5];
    return state;
}

std::expected<void, Si4684Error> Si4684Driver::configureAfterBoot(
    Si4684Band band)
{
    if (band == Si4684Band::Dab) {
        if (auto plan = installDefaultDabFrequencyPlan(); !plan) {
            return plan;
        }
        static constexpr std::uint16_t kDabProps[][2] = {
            {0x0202U, 0x1600U},
            {0x1710U, 0xFC4AU},
            {0x1711U, 0x00F8U},
            {0x8101U, 0x0064U},
            {0xB200U, 0x0000U},
            {0xB201U, 0x0080U},
            {0xB301U, 0x0000U},
            {0xB302U, 0x0000U},
            {0xB303U, 0x0000U},
            {0xB401U, 0x0002U},
            {0xB500U, 0x0000U},
        };
        for (const auto& prop : kDabProps) {
            if (auto set = setProperty(prop[0], prop[1]); !set) {
                return set;
            }
        }
        if (auto xpad = setProperty(kPropDabXpadEnable, 0x0097U); !xpad) {
            return xpad;
        }
    } else {
        if (auto rds = setProperty(kPropFmRdsConfig, 0x0001U); !rds) {
            return rds;
        }
    }

    if (auto i2s = setProperty(kPropDigitalIoOutputSelect, 0x8000U); !i2s) {
        return i2s;
    }
    if (auto rate = setProperty(kPropDigitalIoSampleRate, 0xAC44U); !rate) {
        return rate;
    }
    if (auto pins = setProperty(kPropPinConfigEnable, 0x0003U); !pins) {
        return pins;
    }
    if (auto dabFe = setProperty(kPropDabTuneFeCfg, 0x0001U); !dabFe) {
        return dabFe;
    }
    if (auto dsrv = setProperty(kPropDigitalServiceIntSource, 0x0001U);
        !dsrv) {
        return dsrv;
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::boot(Si4684Band band)
{
    if (booted_ && loadedBand_ == band) {
        return {};
    }

    if (booted_) {
        booted_ = false;
        if (spiDevice_ != nullptr) {
            spi_bus_remove_device(static_cast<spi_device_handle_t>(spiDevice_));
            spiDevice_ = nullptr;
        }
    }

    const core::IFirmwareBlobReader& image =
        (band == Si4684Band::Fm) ? fmImage_ : dabImage_;

    gpio_config_t rstCfg = {};
    rstCfg.pin_bit_mask = 1ULL << pins_.rstbGpio;
    rstCfg.mode = GPIO_MODE_OUTPUT;
    rstCfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    if (gpio_config(&rstCfg) != ESP_OK) {
        return std::unexpected(Si4684Error::ResetFailed);
    }
    gpio_set_level(static_cast<gpio_num_t>(pins_.rstbGpio), 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(static_cast<gpio_num_t>(pins_.rstbGpio), 1);
    vTaskDelay(pdMS_TO_TICKS(3));

    if (!spiBusActive_) {
        spi_bus_config_t busCfg = {};
        busCfg.miso_io_num = pins_.misoGpio;
        busCfg.mosi_io_num = pins_.mosiGpio;
        busCfg.sclk_io_num = pins_.sclkGpio;
        busCfg.quadwp_io_num = -1;
        busCfg.quadhd_io_num = -1;
        busCfg.max_transfer_sz = static_cast<int>(kSpiBufferSize);

        if (spi_bus_initialize(static_cast<spi_host_device_t>(pins_.spiHost),
                               &busCfg, SPI_DMA_CH_AUTO) != ESP_OK) {
            return std::unexpected(Si4684Error::SpiInitFailed);
        }
        spiBusActive_ = true;
    }

    if (spiDevice_ == nullptr) {
        spi_device_interface_config_t devCfg = {};
        devCfg.clock_speed_hz = 10 * 1000 * 1000;
        devCfg.mode = 0;
        devCfg.spics_io_num = pins_.csGpio;
        devCfg.queue_size = 1;

        spi_device_handle_t dev = nullptr;
        if (spi_bus_add_device(static_cast<spi_host_device_t>(pins_.spiHost),
                               &devCfg, &dev) != ESP_OK) {
            return std::unexpected(Si4684Error::SpiInitFailed);
        }
        spiDevice_ = dev;
    }

    if (auto st = writeCommand(Command::GetSysState, nullptr, 0U); !st) {
        return st;
    }

    const std::uint8_t powerUp[] = {
        0x17, 0x48, 0x00, 0xf8, 0x24, 0x01, 0x1F, 0x10,
        0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
    };
    if (auto pu = writeCommand(Command::PowerUp, powerUp, sizeof(powerUp));
        !pu) {
        return std::unexpected(Si4684Error::PowerUpFailed);
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    if (auto li = writeCommand(Command::LoadInit, nullptr, 0U); !li) {
        return std::unexpected(Si4684Error::PatchLoadFailed);
    }

    if (auto patch = hostLoadBlob(patch_, 124U); !patch) {
        return std::unexpected(Si4684Error::PatchLoadFailed);
    }

    vTaskDelay(pdMS_TO_TICKS(4));

    if (auto li2 = writeCommand(Command::LoadInit, nullptr, 0U); !li2) {
        return std::unexpected(Si4684Error::ImageLoadFailed);
    }

    if (auto fw = hostLoadBlob(image, 2044U); !fw) {
        return std::unexpected(Si4684Error::ImageLoadFailed);
    }

    if (auto bootCmd = writeCommand(Command::BootCmd, nullptr, 0U); !bootCmd) {
        return std::unexpected(Si4684Error::BootFailed);
    }

    booted_ = true;
    loadedBand_ = band;

    if (auto cfg = configureAfterBoot(band); !cfg) {
        booted_ = false;
        return cfg;
    }

    ESP_LOGI(kTag, "%s firmware booted",
             band == Si4684Band::Fm ? "FM" : "DAB");
    return {};
}

bool Si4684Driver::isBooted() const noexcept
{
    return booted_;
}

Si4684Band Si4684Driver::loadedBand() const noexcept
{
    return loadedBand_;
}

std::expected<void, Si4684Error> Si4684Driver::tuneFm(
    core::FrequencyKHz frequency)
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return band;
    }
    const std::uint16_t chipFreq = kHzToChipFmFreq(frequency.value());
    const std::uint8_t args[] = {
        0x00U,
        static_cast<std::uint8_t>(chipFreq & 0xFFU),
        static_cast<std::uint8_t>(chipFreq >> 8),
        0x00U,
        0x00U,
    };
    if (auto cmd = writeCommand(Command::FmTuneFreq, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (auto stc = waitStc(); !stc) {
        return stc;
    }
    return {};
}

std::expected<core::FrequencyKHz, Si4684Error> Si4684Driver::seekFm(
    core::SeekDirection direction, SeekBandWrap wrap)
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return std::unexpected(band.error());
    }
    const bool seekUp = direction == core::SeekDirection::Up;
    const bool wrapBand = wrap == SeekBandWrap::Wrap;
    const std::uint8_t args[] = {
        0x10U,
        static_cast<std::uint8_t>(((seekUp ? 1U : 0U) << 1U) | (wrapBand ? 1U : 0U)),
        0x00U,
        0x00U,
        0x00U,
    };
    if (auto cmd = writeCommand(Command::FmSeekStart, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (auto stc = waitStc(); !stc) {
        return std::unexpected(stc.error());
    }
    auto rsq = readFmRsq();
    if (!rsq) {
        return std::unexpected(rsq.error());
    }
    return rsq->frequency;
}

std::expected<Si4684FmRsq, Si4684Error> Si4684Driver::readFmRsq()
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return std::unexpected(band.error());
    }
    const std::uint8_t args[] = {0x00U};
    if (auto cmd = writeCommand(Command::FmRsqStatus, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 23> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    Si4684FmRsq rsq = {};
    const auto khz = chipFmFreqToKHz(readLe16(raw.data() + 6));
    if (auto freq = core::FrequencyKHz::tryFromKhz(khz); freq) {
        rsq.frequency = *freq;
    } else {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    rsq.valid = (raw[4] & 0x01U) != 0U;
    rsq.stereo = (raw[4] & 0x02U) != 0U;
    rsq.rssiDbuV = static_cast<std::int8_t>(raw[8]);
    rsq.snrDb = static_cast<std::int8_t>(raw[9]);
    return rsq;
}

std::expected<Si4684FmRdsStatus, Si4684Error> Si4684Driver::readFmRds()
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return std::unexpected(band.error());
    }
    const std::uint8_t args[] = {0x01U};
    if (auto cmd = writeCommand(Command::FmRdsStatus, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 21> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    Si4684FmRdsStatus rds = {};
    rds.received = (raw[4] & 0x01U) != 0U;
    rds.fifoUsed = raw[10];
    rds.blockA = readLe16(raw.data() + 12);
    rds.blockB = readLe16(raw.data() + 14);
    rds.blockC = readLe16(raw.data() + 16);
    rds.blockD = readLe16(raw.data() + 18);
    return rds;
}

std::expected<std::optional<Si4684DabServiceData>, Si4684Error>
Si4684Driver::readDabServiceData(bool statusOnly, bool ack)
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }

    const std::uint8_t arg1 =
        static_cast<std::uint8_t>((statusOnly ? 0x08U : 0x00U)
                                  | (ack ? 0x01U : 0x00U));
    const std::uint8_t args[] = {arg1};
    if (auto cmd =
            writeCommand(Command::GetDigitalServiceData, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }

    std::array<std::uint8_t, 24> header = {};
    if (auto rd = readRaw(header); !rd) {
        return std::unexpected(rd.error());
    }

    if (statusOnly) {
        if (header[5] == 0U) {
            return std::optional<Si4684DabServiceData>{};
        }
    }

    const std::uint16_t byteCount = readLe16(header.data() + 18);
    if (byteCount == 0U) {
        return std::optional<Si4684DabServiceData>{};
    }
    if (byteCount + 24U > kSpiBufferSize) {
        return std::unexpected(Si4684Error::ReplyTooShort);
    }

    std::vector<std::uint8_t> body(byteCount, 0U);
    if (byteCount > 0U) {
        if (auto rd = readRaw(body); !rd) {
            return std::unexpected(rd.error());
        }
    }

    Si4684DabServiceData data = {};
    data.dataSrc = static_cast<std::uint8_t>((header[7] >> 6U) & 0x03U);
    data.serviceId = readLe32(header.data() + 8);
    data.componentId = readLe32(header.data() + 12);
    data.byteCount = byteCount;
    data.segmentIndex = readLe16(header.data() + 20);
    data.segmentCount = readLe16(header.data() + 22);
    data.payload = std::move(body);
    return data;
}

std::expected<void, Si4684Error> Si4684Driver::installDefaultDabFrequencyPlan()
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return band;
    }

    std::array<std::uint8_t, 4U + kDefaultDabFrequencyKhz.size() * 4U> cmd =
        {};
    cmd[0] = static_cast<std::uint8_t>(Command::DabSetFreqList);
    cmd[1] = static_cast<std::uint8_t>(kDefaultDabFrequencyKhz.size());
    cmd[2] = 0x00U;
    cmd[3] = 0x00U;
    for (std::size_t i = 0; i < kDefaultDabFrequencyKhz.size(); ++i) {
        const std::uint32_t hz = kDefaultDabFrequencyKhz[i];
        const std::size_t off = 4U + i * 4U;
        cmd[off] = static_cast<std::uint8_t>(hz & 0xFFU);
        cmd[off + 1] = static_cast<std::uint8_t>((hz >> 8) & 0xFFU);
        cmd[off + 2] = static_cast<std::uint8_t>((hz >> 16) & 0xFFU);
        cmd[off + 3] = static_cast<std::uint8_t>(hz >> 24);
    }
    return sendCommand(cmd);
}

std::expected<void, Si4684Error> Si4684Driver::tuneDab(std::uint8_t freqIndex)
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return band;
    }
    if (freqIndex >= kDefaultDabFrequencyKhz.size()) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    const std::uint8_t args[] = {0x00U, freqIndex, 0x00U, 0x00U, 0x00U};
    if (auto cmd = writeCommand(Command::DabTuneFreq, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (auto stc = waitStc(); !stc) {
        return stc;
    }
    return {};
}

std::expected<Si4684DabDigRadStatus, Si4684Error>
Si4684Driver::readDabDigRadStatus()
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }
    const std::uint8_t args[] = {0x01U};
    if (auto cmd =
            writeCommand(Command::DabDigRadStatus, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 20> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    Si4684DabDigRadStatus status = {};
    status.ficQuality = raw[9];
    status.cnrDb = raw[10];
    status.acquired = (raw[4] & 0x08U) != 0U;
    status.valid = status.ficQuality > 0U;
    return status;
}

std::expected<Si4684DabEventStatus, Si4684Error>
Si4684Driver::readDabEventStatus()
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }
    const std::uint8_t args[] = {0x00U};
    if (auto cmd =
            writeCommand(Command::DabGetEventStatus, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 9> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    Si4684DabEventStatus events = {};
    events.serviceListReady = (raw[4] & 0x01U) != 0U;
    events.reconfig = (raw[4] & 0x02U) != 0U;
    return events;
}

std::expected<std::vector<Si4684DabService>, Si4684Error>
Si4684Driver::fetchDabServiceList()
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }

    const std::uint8_t args[] = {0x00U};
    if (auto cmd = writeCommand(Command::GetDigitalServiceList, args,
                                sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }

    std::array<std::uint8_t, 9> header = {};
    if (auto rd = readRaw(header); !rd) {
        return std::unexpected(rd.error());
    }

    const std::uint16_t payloadSize = readLe16(header.data() + 5);
    if (payloadSize == 0U || payloadSize + 6U > kSpiBufferSize) {
        return std::unexpected(Si4684Error::ReplyTooShort);
    }

    std::vector<std::uint8_t> body(payloadSize + 6U, 0U);
    if (auto rd = readRaw(body); !rd) {
        return std::unexpected(rd.error());
    }

    const std::uint8_t serviceCount = body[9];
    std::vector<Si4684DabService> services;
    services.reserve(serviceCount);

    std::size_t offset = 13U;
    for (std::uint8_t i = 0; i < serviceCount; ++i) {
        if (offset + 24U > body.size()) {
            break;
        }
        Si4684DabService entry = {};
        entry.serviceId = readLe32(body.data() + offset);
        offset += 4U;
        entry.serviceType = body[offset] & 0x3FU;
        const std::uint8_t componentCount = body[offset + 1] & 0x0FU;
        offset += 4U;
        std::memcpy(entry.label.data(), body.data() + offset, 16U);
        entry.label[16] = '\0';
        offset += 16U;

        if (componentCount > 0U && offset + 4U <= body.size()) {
            entry.componentId = readLe32(body.data() + offset);
            offset += 4U;
            if (offset < body.size()) {
                ++offset;
            }
        }
        services.push_back(entry);
    }
    return services;
}

std::expected<void, Si4684Error> Si4684Driver::startDabService(
    std::uint32_t serviceId,
    std::uint32_t componentId,
    Si4684DigitalServiceType type)
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return band;
    }
    const std::uint8_t args[] = {
        static_cast<std::uint8_t>(type),
        0x00U,
        0x00U,
        static_cast<std::uint8_t>(serviceId & 0xFFU),
        static_cast<std::uint8_t>((serviceId >> 8) & 0xFFU),
        static_cast<std::uint8_t>((serviceId >> 16) & 0xFFU),
        static_cast<std::uint8_t>(serviceId >> 24),
        static_cast<std::uint8_t>(componentId & 0xFFU),
        static_cast<std::uint8_t>((componentId >> 8) & 0xFFU),
        static_cast<std::uint8_t>((componentId >> 16) & 0xFFU),
        static_cast<std::uint8_t>(componentId >> 24),
    };
    if (auto cmd =
            writeCommand(Command::StartDigitalService, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    return {};
}

} // namespace si4684
