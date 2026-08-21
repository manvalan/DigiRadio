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
#include "esp_heap_caps.h"

namespace si4684 {

namespace {

constexpr char kTag[] = "Si4684";
constexpr std::size_t kSpiBufferSize = 4096U;
constexpr int kCtsPollMs = 2;
constexpr int kCtsRetries = 5000;
constexpr int kStcRetries = 250;
constexpr int kStcPollMs = 20;
/** SPI readRaw: byte 0 is a lead-in; STATUS0 is at index 1 (AN649). */
constexpr std::size_t kSpiReplyLeadIn = 1U;
/** FM_RSQ_STATUS field indices with kSpiReplyLeadIn (AN649 RESP5–10). */
constexpr std::size_t kFmRsqOffValid = 6U;
constexpr std::size_t kFmRsqOffReadFreq = 7U;
constexpr std::size_t kFmRsqOffRssi = 10U;
constexpr std::size_t kFmRsqOffSnr = 11U;

constexpr std::uint16_t kPropDigitalIoOutputSelect = 0x0200U;
constexpr std::uint16_t kPropDigitalIoOutputFormat = 0x0202U;
constexpr std::uint16_t kPropDigitalIoSampleRate = 0x0201U;
/** I2S slave — ADAU1701 is bus master (AN649 property 0x0200, bit15=0). */
constexpr std::uint16_t kSi4684I2sSlaveSelect = 0x0000U;
/** 24-bit samples in 32-bit I2S slots (SAMPL=0x18, SLOT=0x7, I2S mode). */
constexpr std::uint16_t kSi4684I2sOutputFormat = 0x1870U;
/** 48000 Hz — matches ADAU1701 48 kHz master clock domain. */
constexpr std::uint16_t kSi4684I2sSampleRateHz = 0xBB80U;
constexpr std::uint16_t kPropPinConfigEnable = 0x0800U;
constexpr std::uint16_t kPropAudioVolume = 0x0300U;
constexpr std::uint16_t kPropAudioMute = 0x0301U;
constexpr std::uint16_t kPropAudioOutputConfig = 0x0302U;
/** AN649 PIN_CONFIG_ENABLE bit1 I2SOUTEN + bit15 INTBOUTEN — matches the
 *  value used by hitech95/si468x_dab_receiver's working ALSA codec driver
 *  (SI468X_PROP_I2S_ENABLED = 0x8002); INTBOUTEN alone (0x0002) was not
 *  sufficient to produce audio on real hardware in this project. */
constexpr std::uint16_t kSi4684I2sOutEnable = 0x8002U;
/** Si4684 volume: 0=mute, 63=max (AN649 AUDIO_ANALOG_VOLUME). */
constexpr std::uint8_t kSi4684VolumeMax = 63U;
constexpr std::uint16_t kPropFmRdsConfig = 0x3C02U;
/** AN649 FM valid tune properties (defaults RSSI 17 dBµV, SNR 10 dB). */
constexpr std::uint16_t kPropFmValidRssiThreshold = 0x3202U;
constexpr std::uint16_t kPropFmValidSnrThreshold = 0x3204U;
constexpr std::uint16_t kFmValidRssiThresholdDbuV = 0x0005U;
constexpr std::uint16_t kFmValidSnrThresholdDb = 0x0003U;
/** AN649 FM seek band/spacing (10 kHz units): 87.5–107.9 MHz, 100 kHz steps. */
constexpr std::uint16_t kPropFmSeekBandBottom = 0x3100U;
constexpr std::uint16_t kPropFmSeekBandTop = 0x3101U;
constexpr std::uint16_t kPropFmSeekSpacing = 0x3102U;
constexpr std::uint16_t kFmSeekBandBottomChip = 8750U;
constexpr std::uint16_t kFmSeekBandTopChip = 10790U;
constexpr std::uint16_t kFmSeekSpacingChip = 10U;
/** AN851 §"Varactor Tuning Properties" recommended-network table: FM slope/
 *  intercept for 0x1710/0x1711 (Table, "FM" row: 0xEDB5 / 0x01E3). */
constexpr std::uint16_t kFmTuneFeVarm = 0xEDB5U;
constexpr std::uint16_t kFmTuneFeVarb = 0x01E3U;
constexpr std::uint16_t kFmTuneFeCfgEnable = 0x0001U;
constexpr std::uint16_t kPropDabTuneFeCfg = 0x1712U;
constexpr std::uint16_t kPropFmTuneFeCfg = 0x1712U;
constexpr std::uint16_t kPropDabXpadEnable = 0xB400U;
constexpr std::uint16_t kPropDigitalServiceIntSource = 0x8100U;
constexpr std::uint16_t kPropDabEventIntSource = 0xB300U;
/** AN649 INT_CTL_ENABLE / INT_CTL_REPEAT — route STC to INTB until STCACK. */
constexpr std::uint16_t kPropIntCtlEnable = 0x0000U;
constexpr std::uint16_t kPropIntCtlRepeat = 0x0001U;
constexpr std::uint16_t kIntCtlStcEnable = 0x0001U;   ///< STCIEN
constexpr std::uint16_t kIntCtlStcRepeat = 0x0001U;   ///< STCREP

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

std::expected<bool, Si4684Error> Si4684Driver::pollStc()
{
    std::array<std::uint8_t, 5> pollTx = {};
    std::array<std::uint8_t, 5> pollRx = {};
    spi_transaction_t txn = {};
    txn.length = pollTx.size() * 8U;
    txn.tx_buffer = pollTx.data();
    txn.rx_buffer = pollRx.data();
    if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                            &txn) != ESP_OK) {
        return std::unexpected(Si4684Error::SpiInitFailed);
    }
    // STATUS0 STCINT (AN649 D0) is at pollRx[1] after the SPI lead-in byte.
    if ((pollRx[1] & 0x01U) != 0U) {
        return true;
    }
    if (pins_.intbGpio >= 0) {
        const gpio_num_t intb = static_cast<gpio_num_t>(pins_.intbGpio);
        if (gpio_get_level(intb) == 0) {
            return true;
        }
    }
    return false;
}

std::expected<void, Si4684Error> Si4684Driver::waitStc(int maxRetries)
{
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(kStcPollMs));
        auto stc = pollStc();
        if (!stc) {
            return std::unexpected(stc.error());
        }
        if (*stc) {
            return {};
        }
    }
    // Diagnostic: dump the final poll so a STC timeout is distinguishable
    // between "chip replies but STCINT never sets" (register/offset bug)
    // and "chip stopped replying" (SPI/CTS problem) without guessing.
    std::array<std::uint8_t, 5> pollTx = {};
    std::array<std::uint8_t, 5> pollRx = {};
    spi_transaction_t txn = {};
    txn.length = pollTx.size() * 8U;
    txn.tx_buffer = pollTx.data();
    txn.rx_buffer = pollRx.data();
    const esp_err_t spiResult = spi_device_transmit(
        static_cast<spi_device_handle_t>(spiDevice_), &txn);
    const int intbLevel = pins_.intbGpio >= 0
        ? gpio_get_level(static_cast<gpio_num_t>(pins_.intbGpio))
        : -1;
    ESP_LOGW(kTag,
             "STC timeout: last poll spi_err=%d status=%02x %02x %02x %02x "
             "%02x INTB=%d",
             static_cast<int>(spiResult), pollRx[0], pollRx[1], pollRx[2],
             pollRx[3], pollRx[4], intbLevel);
    return std::unexpected(Si4684Error::StcTimeout);
}

std::expected<void, Si4684Error> Si4684Driver::clearFmStc()
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return band;
    }
    // AN649 Command 0x32 FM_RSQ_STATUS has a single argument, ARG1, whose
    // bit 0 is STCACK (clears a latched STCINT). No ARG2+ exists for this
    // command.
    constexpr std::uint8_t kStcAck = 0x01U;
    if (auto cmd = writeCommand(Command::FmRsqStatus, nullptr, 0U, kStcAck);
        !cmd) {
        return cmd;
    }
    std::array<std::uint8_t, 23> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return rd;
    }
    return {};
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
    Command cmd, const std::uint8_t* payload, std::size_t length,
    std::uint8_t arg1)
{
    if (length + 2U > kSpiBufferSize) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, kSpiBufferSize> buffer = {};
    buffer[0] = static_cast<std::uint8_t>(cmd);
    buffer[1] = arg1;
    if (payload != nullptr && length > 0U) {
        std::memcpy(buffer.data() + 2U, payload, length);
    }
    return sendCommand({buffer.data(), 2U + length});
}

std::expected<void, Si4684Error> Si4684Driver::hostLoadBlob(
    const core::IFirmwareBlobReader& blob, std::size_t chunkPayload)
{
    // Buffer DMA-capable e allineato: obbligatorio per spi_device_transmit
    // con trasferimenti grandi. Un buffer sullo stack non e' DMA-safe e puo'
    // corrompere i dati sui blob grossi (patch/firmware).
    const std::size_t txSize = 4U + chunkPayload;
    auto* tx = static_cast<std::uint8_t*>(
        heap_caps_malloc(txSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (tx == nullptr) {
        return std::unexpected(Si4684Error::ImageLoadFailed);
    }

    std::array<std::byte, 2044> payload = {};
    std::size_t offset = 0U;
    Si4684Error err = Si4684Error::ImageLoadFailed;
    bool failed = false;

    while (offset < blob.size()) {
        const std::size_t maxChunk = std::min(chunkPayload, payload.size());
        const std::size_t copied =
            blob.read(offset, std::span<std::byte>(payload.data(), maxChunk));
        if (copied == 0U) {
            failed = true;
            break;
        }

        std::memset(tx, 0, txSize);
        tx[0] = static_cast<std::uint8_t>(Command::HostLoad);
        tx[1] = 0x00U;
        tx[2] = 0x00U;
        tx[3] = 0x00U;
        std::memcpy(tx + 4U, payload.data(), copied);

        spi_transaction_t txn = {};
        txn.length = txSize * 8U;
        txn.tx_buffer = tx;
        if (spi_device_transmit(static_cast<spi_device_handle_t>(spiDevice_),
                                &txn) != ESP_OK) {
            failed = true;
            break;
        }
        if (auto cts = waitCts(); !cts) {
            err = cts.error();
            failed = true;
            break;
        }
        offset += copied;
    }

    heap_caps_free(tx);
    if (failed) {
        ESP_LOGW(kTag, "blob stream failed: %u/%u bytes sent",
                 static_cast<unsigned>(offset),
                 static_cast<unsigned>(blob.size()));
        return std::unexpected(err);
    }
    ESP_LOGI(kTag, "blob streamed: %u/%u bytes",
             static_cast<unsigned>(offset), static_cast<unsigned>(blob.size()));
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

    // readRaw() replies carry a one-byte SPI lead-in before STATUS0 -- the
    // same convention already confirmed and documented in pollStc() below
    // (raw[0]=lead-in, raw[1]=STATUS0 ... raw[4]=STATUS3, raw[5]=RESP4).
    // The 24/13-byte buffer sizes above already account for this lead-in
    // byte (23/12 real response bytes + 1); only the field indices need it.
    // GET_PART_INFO (Cmd 0x08): PART[15:0] = RESP8/RESP9 = raw[9]/raw[10].
    // GET_FUNC_INFO (Cmd 0x12): REVEXT/REVBRANCH/REVINT = RESP4/5/6 =
    // fnRaw[5]/[6]/[7]; SVNID[31:0] = RESP8-11 = fnRaw[9..12] (little-endian).
    Si4684PartInfo info = {};
    info.chipId = readLe16(raw.data() + 9U);
    info.firmwareMajor = fnRaw[5];
    info.firmwareMinor = fnRaw[6];
    info.firmwareBuild = fnRaw[7];
    info.svnId = readLe32(fnRaw.data() + 9U);
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
    // readRaw() replies carry a one-byte SPI lead-in before STATUS0 (see the
    // comment on getPartInfo() above): raw[0]=lead-in, raw[4]=STATUS3,
    // raw[5]=RESP4=IMAGE. The 7-byte buffer above already accounts for it
    // (STATUS0-3 + RESP4-5 + 1 lead-in = 7).
    Si4684SysState state = {};
    state.imageType = raw[5];
    return state;
}

std::expected<void, Si4684Error> Si4684Driver::configureAfterBoot(
    Si4684Band band)
{
    if (auto stcEn = setProperty(kPropIntCtlEnable, kIntCtlStcEnable); !stcEn) {
        return stcEn;
    }
    if (auto stcRep = setProperty(kPropIntCtlRepeat, kIntCtlStcRepeat); !stcRep) {
        return stcRep;
    }

    if (band == Si4684Band::Dab) {
        if (auto plan = installDefaultDabFrequencyPlan(); !plan) {
            return plan;
        }
        // AN851 recommended-network table, "DAB" row: 0xF8A9 / 0x01C6.
        static constexpr std::uint16_t kDabProps[][2] = {
            {0x1710U, 0xF8A9U},
            {0x1711U, 0x01C6U},
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
        if (auto dabFe = setProperty(kPropDabTuneFeCfg, 0x0001U); !dabFe) {
            ESP_LOGW(kTag, "DAB TUNE_FE_CFG (0x1712) failed");
            return dabFe;
        }
        if (auto dsrv = setProperty(kPropDigitalServiceIntSource, 0x0001U);
            !dsrv) {
            ESP_LOGW(kTag, "DIGITAL_SERVICE_INT_SOURCE (0x8100) failed");
            return dsrv;
        }
        // AN649 Property 0xB300 DAB_EVENT_INTERRUPT_SOURCE, bit0=SRVLIST_INTEN
        // (default 0x0000 = disabled at power-on). Without this, nothing in
        // this driver ever enables the service-list-ready event, so
        // fetchDabServiceList() can stay gated behind an eternally-false
        // serviceListReady even on a clean, well-locked ensemble.
        if (auto evt = setProperty(kPropDabEventIntSource, 0x0001U); !evt) {
            ESP_LOGW(kTag, "DAB_EVENT_INTERRUPT_SOURCE (0xB300) failed");
            return evt;
        }
    } else {
    // FM varactor cal per hitech95/uGreen DTS (not DAB PE5PVB values).
        static constexpr std::uint16_t kFmFeProps[][2] = {
            {0x1710U, kFmTuneFeVarm},
            {0x1711U, kFmTuneFeVarb},
        };
        for (const auto& prop : kFmFeProps) {
            if (auto set = setProperty(prop[0], prop[1]); !set) {
                return set;
            }
        }
        if (auto feCfg = setProperty(kPropFmTuneFeCfg, kFmTuneFeCfgEnable);
            !feCfg) {
            return feCfg;
        }
        static constexpr std::uint16_t kFmSeekProps[][2] = {
            {kPropFmSeekBandBottom, kFmSeekBandBottomChip},
            {kPropFmSeekBandTop, kFmSeekBandTopChip},
            {kPropFmSeekSpacing, kFmSeekSpacingChip},
        };
        for (const auto& prop : kFmSeekProps) {
            if (auto set = setProperty(prop[0], prop[1]); !set) {
                return set;
            }
        }
        if (auto rds = setProperty(kPropFmRdsConfig, 0x0001U); !rds) {
            return rds;
        }
        // AN649 §0x3202/0x3204: lower seek/tune validity for weak lab antennas.
        if (auto rssi = setProperty(kPropFmValidRssiThreshold,
                                    kFmValidRssiThresholdDbuV);
            !rssi) {
            return rssi;
        }
        if (auto snr = setProperty(kPropFmValidSnrThreshold,
                                   kFmValidSnrThresholdDb);
            !snr) {
            return snr;
        }
        ESP_LOGI(kTag, "FM valid tune: RSSI>=%u dBuV SNR>=%u dB",
                 static_cast<unsigned>(kFmValidRssiThresholdDbuV),
                 static_cast<unsigned>(kFmValidSnrThresholdDb));
    }

    if (auto i2sRole =
            setProperty(kPropDigitalIoOutputSelect, kSi4684I2sSlaveSelect);
        !i2sRole) {
        return i2sRole;
    }
    if (auto i2sFmt =
            setProperty(kPropDigitalIoOutputFormat, kSi4684I2sOutputFormat);
        !i2sFmt) {
        return i2sFmt;
    }
    if (auto rate =
            setProperty(kPropDigitalIoSampleRate, kSi4684I2sSampleRateHz);
        !rate) {
        return rate;
    }
    // AN649 Property 0x0800 PIN_CONFIG_ENABLE bit1=I2SOUTEN, bit0=DACOUTEN:
    // "only I2SOUTEN or DACOUTEN can be enabled at a time. If both enabled,
    // only analog audio output is enabled." We only wire I2S to the
    // ADAU1701 (no DAC pins connected), so DACOUTEN must stay 0 or the chip
    // silently falls back to analog-only and the I2S bus carries silence.
    if (auto pins = setProperty(kPropPinConfigEnable, kSi4684I2sOutEnable);
        !pins) {
        return pins;
    }
    if (auto mute = setProperty(kPropAudioMute, 0x0000U); !mute) {
        return mute;
    }
    // AN649 Property 0x0302 AUDIO_OUTPUT_CONFIG bit0=MONO (all other bits
    // reserved, must be 0). Not an I2S enable — that lives at 0x0800 above.
    if (auto outCfg = setProperty(kPropAudioOutputConfig, 0x0000U); !outCfg) {
        return outCfg;
    }
    if (auto vol = setProperty(kPropAudioVolume, kSi4684VolumeMax); !vol) {
        return vol;
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::boot(
    Si4684Band band, std::uint8_t xtalIbias, std::uint8_t xtalCtun)
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
    vTaskDelay(pdMS_TO_TICKS(20));

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

    if (pins_.intbGpio >= 0) {
        gpio_config_t intCfg = {};
        intCfg.pin_bit_mask = 1ULL << pins_.intbGpio;
        intCfg.mode = GPIO_MODE_INPUT;
        intCfg.pull_up_en = GPIO_PULLUP_ENABLE;
        if (gpio_config(&intCfg) != ESP_OK) {
            return std::unexpected(Si4684Error::SpiInitFailed);
        }
    }

    if (auto st = writeCommand(Command::GetSysState, nullptr, 0U); !st) {
        return st;
    }

    // ARG2=0x17(CLK_MODE=crystal,TR_SIZE), ARG3=IBIAS, ARG4-7=XTAL_FREQ
    // 19.2 MHz (0x0124F800), ARG8=CTUN, ARG9=0x10 (fixed bit4=1 per AN649
    // §Command 0x01), ARG10-15=0 (AN649 POWER_UP argument table).
    std::uint8_t powerUp[] = {
        0x17, xtalIbias, 0x00, 0xf8, 0x24, 0x01, xtalCtun, 0x10,
        0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
    };
    if (auto pu = writeCommand(Command::PowerUp, powerUp, sizeof(powerUp));
        !pu) {
        return std::unexpected(Si4684Error::PowerUpFailed);
    }

    vTaskDelay(pdMS_TO_TICKS(20));

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

    // Blob integrity/identity diagnostic: confirms the loaded image is a
    // real, complete Si4684 firmware (non-zero, sane version numbers) and
    // which application actually took over the command interpreter after
    // BOOT_CMD, rather than assuming it from what we intended to load.
    if (auto sys = getSysState(); sys) {
        ESP_LOGI(kTag, "GET_SYS_STATE: image=%u",
                 static_cast<unsigned>(sys->imageType));
    } else {
        ESP_LOGW(kTag, "GET_SYS_STATE failed (err=%d)",
                 static_cast<int>(sys.error()));
    }
    if (auto info = getPartInfo(); info) {
        ESP_LOGI(kTag,
                 "GET_PART_INFO/GET_FUNC_INFO: part=%u rev=%u.%u.%u "
                 "svnid=0x%08x",
                 static_cast<unsigned>(info->chipId),
                 static_cast<unsigned>(info->firmwareMajor),
                 static_cast<unsigned>(info->firmwareMinor),
                 static_cast<unsigned>(info->firmwareBuild),
                 static_cast<unsigned>(info->svnId));
    } else {
        ESP_LOGW(kTag, "GET_PART_INFO failed (err=%d)",
                 static_cast<int>(info.error()));
    }
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
    core::FrequencyKHz frequency, std::uint8_t antCap)
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return band;
    }
    if (auto cleared = clearFmStc(); !cleared) {
        return cleared;
    }
    const std::uint16_t chipFreq = kHzToChipFmFreq(frequency.value());
    // writeCommand() always prepends a fixed ARG1=0x00 (DIR_TUNE=0,
    // TUNE_MODE=0, INJECTION=0), so this array starts at ARG2 (AN649
    // Command 0x30 table: ARG2=FREQ[7:0], ARG3=FREQ[15:8], ARG4=ANTCAP[7:0],
    // ARG5=ANTCAP[15:8], ARG6=PROG_ID). Do not add a leading/trailing byte
    // here or every field shifts into the wrong ARG slot.
    const std::uint8_t args[] = {
        static_cast<std::uint8_t>(chipFreq & 0xFFU),
        static_cast<std::uint8_t>(chipFreq >> 8),
        antCap, // ANTCAP[7:0] -- 0 = auto (FE_VARM/VARB), else forced value
        0x00U,  // ANTCAP[15:8] -- range is 0-128, high byte always 0
        0x00U, // PROG_ID (AN649 ARG6; ignored when DIR_TUNE=0)
    };
    if (auto cmd = writeCommand(Command::FmTuneFreq, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (auto stc = waitStc(kStcRetries); !stc) {
        ESP_LOGW(kTag, "FM tune STC timeout at %u kHz — settling 150 ms",
                 static_cast<unsigned>(frequency.value()));
        vTaskDelay(pdMS_TO_TICKS(150));
    } else {
        (void)clearFmStc();
    }
    if (auto rsq = readFmRsq(); rsq) {
        const std::uint32_t readKhz =
            rsq->frequency ? rsq->frequency->value() : 0U;
        if (readKhz != 0U && readKhz != frequency.value()) {
            ESP_LOGW(kTag,
                     "FM tune READFREQ mismatch: want %u kHz got %u kHz",
                     static_cast<unsigned>(frequency.value()),
                     static_cast<unsigned>(readKhz));
        }
        ESP_LOGI(kTag,
                 "FM tuned %u kHz antcap=%u rssi=%d dBuV snr=%d dB valid=%d "
                 "readfreq=%u",
                 static_cast<unsigned>(frequency.value()),
                 static_cast<unsigned>(antCap),
                 static_cast<int>(rsq->rssiDbuV),
                 static_cast<int>(rsq->snrDb),
                 static_cast<int>(rsq->valid),
                 static_cast<unsigned>(readKhz));
    } else {
        ESP_LOGI(kTag, "FM tuned %u kHz (RSQ read failed)",
                 static_cast<unsigned>(frequency.value()));
    }
    return {};
}

std::expected<core::FrequencyKHz, Si4684Error> Si4684Driver::seekFm(
    core::SeekDirection direction, SeekBandWrap wrap)
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return std::unexpected(band.error());
    }
    if (auto cleared = clearFmStc(); !cleared) {
        return std::unexpected(cleared.error());
    }
    std::optional<std::uint32_t> prevKhz;
    if (auto before = readFmRsq(); before && before->frequency) {
        prevKhz = before->frequency->value();
    }
    const bool seekUp = direction == core::SeekDirection::Up;
    const bool wrapBand = wrap == SeekBandWrap::Wrap;
    // AN649 Command 0x31 FM_SEEK_START: ARG1=tune_mode/injection (default
    // 0x00 here), ARG2=SEEKUP|WRAP, ARG3=0x00 fixed, ARG4=ANTCAP[7:0],
    // ARG5=ANTCAP[15:8]. writeCommand() supplies ARG1, so this array starts
    // at ARG2.
    const std::uint8_t seekFlags =
        static_cast<std::uint8_t>(((seekUp ? 1U : 0U) << 1U)
                                  | (wrapBand ? 1U : 0U));
    const std::uint8_t args[] = {
        seekFlags,
        0x00U,
        0x00U,
        0x00U,
    };
    if (auto cmd = writeCommand(Command::FmSeekStart, args, sizeof(args));
        !cmd) {
        ESP_LOGW(kTag, "FM seek command failed (flags=0x%02x)",
                 static_cast<unsigned>(seekFlags));
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (auto stc = waitStc(kStcRetries); !stc) {
        ESP_LOGW(kTag, "FM seek STC timeout (flags=0x%02x)",
                 static_cast<unsigned>(seekFlags));
        return std::unexpected(stc.error());
    }
    (void)clearFmStc();
    auto rsq = readFmRsq();
    if (!rsq) {
        ESP_LOGW(kTag, "FM seek RSQ read failed");
        return std::unexpected(rsq.error());
    }
    if (!rsq->frequency) {
        ESP_LOGW(kTag, "FM seek READFREQ out of band (valid=%d)",
                 static_cast<int>(rsq->valid));
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (prevKhz && rsq->frequency->value() == *prevKhz) {
        ESP_LOGW(kTag, "FM seek READFREQ unchanged at %u kHz",
                 static_cast<unsigned>(*prevKhz));
        return std::unexpected(Si4684Error::TuneFailed);
    }
    return *rsq->frequency;
}

std::expected<Si4684FmRsq, Si4684Error> Si4684Driver::readFmRsq()
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return std::unexpected(band.error());
    }
    // AN649 Command 0x32 FM_RSQ_STATUS has a single argument, ARG1 (all
    // ack/cancel bits 0 here — a plain status read). No ARG2+ exists.
    if (auto cmd = writeCommand(Command::FmRsqStatus, nullptr, 0U); !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 23> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    if (raw.size() < kFmRsqOffSnr + 1U) {
        return std::unexpected(Si4684Error::ReplyTooShort);
    }
    ESP_LOGI(kTag,
             "FM RSQ raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
             "%02x %02x",
             raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7],
             raw[8], raw[9], raw[10], raw[11]);
    const auto khz =
        chipFmFreqToKHz(readLe16(raw.data() + kFmRsqOffReadFreq));
    const auto freq = core::FrequencyKHz::tryFromKhz(khz);
    const bool freqInBand = static_cast<bool>(freq);
    const bool chipValid = (raw[kFmRsqOffValid] & 0x01U) != 0U;
    if (!freqInBand && khz != 0U) {
        ESP_LOGW(kTag,
                 "FM RSQ out-of-band freq %u kHz (st=%02x %02x %02x %02x "
                 "freq=%02x %02x rssi=%02x snr=%02x)",
                 static_cast<unsigned>(khz), raw[kSpiReplyLeadIn],
                 raw[kSpiReplyLeadIn + 1U],
                 raw[kSpiReplyLeadIn + 2U], raw[kSpiReplyLeadIn + 3U],
                 raw[kFmRsqOffReadFreq], raw[kFmRsqOffReadFreq + 1U],
                 raw[kFmRsqOffRssi], raw[kFmRsqOffSnr]);
    } else if (khz == 0U && raw[kSpiReplyLeadIn] == 0U
               && raw[kSpiReplyLeadIn + 1U] == 0U) {
        ESP_LOGW(kTag, "FM RSQ empty reply (st=%02x %02x %02x %02x)",
                 raw[kSpiReplyLeadIn], raw[kSpiReplyLeadIn + 1U],
                 raw[kSpiReplyLeadIn + 2U], raw[kSpiReplyLeadIn + 3U]);
    }
    Si4684FmRsq rsq{
        freqInBand ? std::optional<core::FrequencyKHz>{*freq} : std::nullopt,
        static_cast<std::int8_t>(raw[kFmRsqOffRssi]),
        static_cast<std::int8_t>(raw[kFmRsqOffSnr]),
        freqInBand && chipValid,
        false,
    };
    return rsq;
}

std::expected<Si4684FmRdsStatus, Si4684Error> Si4684Driver::readFmRds()
{
    if (auto band = ensureBand(Si4684Band::Fm); !band) {
        return std::unexpected(band.error());
    }
    // AN649 Command 0x34 FM_RDS_STATUS has a single argument, ARG1: bit0
    // INTACK (clear RDSINT). No ARG2+ exists.
    constexpr std::uint8_t kIntAck = 0x01U;
    if (auto cmd = writeCommand(Command::FmRdsStatus, nullptr, 0U, kIntAck);
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 21> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    // raw[0]=lead-in, raw[1..4]=STATUS0-3, raw[5]=RESP4 (established
    // convention, see getPartInfo()/readDabDigRadStatus() comments) — every
    // offset below is RESP-number relative to that, not raw[4].
    Si4684FmRdsStatus rds = {};
    rds.received = (raw[5] & 0x01U) != 0U;
    rds.fifoUsed = raw[11];
    rds.blockA = readLe16(raw.data() + 13U);
    rds.blockB = readLe16(raw.data() + 15U);
    rds.blockC = readLe16(raw.data() + 17U);
    rds.blockD = readLe16(raw.data() + 19U);
    return rds;
}

std::expected<std::optional<Si4684DabServiceData>, Si4684Error>
Si4684Driver::readDabServiceData(bool statusOnly, bool ack)
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }

    // AN649 Command 0x84 GET_DIGITAL_SERVICE_DATA has a single argument,
    // ARG1: bit4 STATUS_ONLY, bit0 ACK. No ARG2+ exists.
    const std::uint8_t arg1 =
        static_cast<std::uint8_t>((statusOnly ? 0x10U : 0x00U)
                                  | (ack ? 0x01U : 0x00U));
    if (auto cmd =
            writeCommand(Command::GetDigitalServiceData, nullptr, 0U, arg1);
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }

    // raw[5]=RESP4 (see readFmRds()). AN649 Command 0x84 response:
    // RESP4=flags, RESP5=BUFF_COUNT, RESP6=SRV_STATE, RESP7=DATA_SRC/DSCTy,
    // RESP8-11=SERVICE_ID, RESP12-15=COMP_ID, RESP16-17=UATYPE,
    // RESP18-19=BYTE_COUNT, RESP20-21=SEG_NUM, RESP22-23=NUM_SEGS — 25
    // header bytes total (lead-in + STATUS0-3 + RESP4-23).
    std::array<std::uint8_t, 25> header = {};
    if (auto rd = readRaw(header); !rd) {
        return std::unexpected(rd.error());
    }

    if (statusOnly) {
        if (header[5] == 0U) {
            return std::optional<Si4684DabServiceData>{};
        }
    }

    const std::uint16_t byteCount = readLe16(header.data() + 19);
    if (byteCount == 0U) {
        return std::optional<Si4684DabServiceData>{};
    }
    if (byteCount + 25U > kSpiBufferSize) {
        return std::unexpected(Si4684Error::ReplyTooShort);
    }

    std::vector<std::uint8_t> body(byteCount, 0U);
    if (byteCount > 0U) {
        if (auto rd = readRaw(body); !rd) {
            return std::unexpected(rd.error());
        }
    }

    Si4684DabServiceData data = {};
    data.dataSrc = static_cast<std::uint8_t>((header[8] >> 6U) & 0x03U);
    data.serviceId = readLe32(header.data() + 9);
    data.componentId = readLe32(header.data() + 13);
    data.byteCount = byteCount;
    data.segmentIndex = readLe16(header.data() + 21);
    data.segmentCount = readLe16(header.data() + 23);
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

std::expected<void, Si4684Error> Si4684Driver::tuneDab(
    std::uint8_t freqIndex, std::uint8_t antCap)
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return band;
    }
    if (freqIndex >= kDefaultDabFrequencyKhz.size()) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    // writeCommand() always prepends a fixed ARG1=0x00 (INJECTION=0), so
    // this array starts at ARG2 (AN649 Command 0xB0 table: ARG2=FREQ_INDEX,
    // ARG3=0x00 fixed, ARG4=ANTCAP[7:0], ARG5=ANTCAP[15:8] -- high byte
    // always 0, range is 0-128, same as tuneFm's ANTCAP).
    const std::uint8_t args[] = {freqIndex, 0x00U, antCap, 0x00U};
    if (auto cmd = writeCommand(Command::DabTuneFreq, args, sizeof(args));
        !cmd) {
        return std::unexpected(Si4684Error::TuneFailed);
    }
    if (auto stc = waitStc(kStcRetries); !stc) {
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
    // AN649 Command 0xB2 DAB_DIGRAD_STATUS has a single argument, ARG1:
    // bit0 STC_ACK (clears the STC interrupt). No ARG2+ exists.
    constexpr std::uint8_t kStcAck = 0x01U;
    if (auto cmd =
            writeCommand(Command::DabDigRadStatus, nullptr, 0U, kStcAck);
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
    // raw[5]=RESP4 (see readFmRds()); ACQINT is RESP4 bit3.
    status.acquired = (raw[5] & 0x08U) != 0U;
    status.valid = status.ficQuality > 0U;
    return status;
}

std::expected<Si4684DabEventStatus, Si4684Error>
Si4684Driver::readDabEventStatus()
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }
    // AN649 Command 0xB3 DAB_GET_EVENT_STATUS has a single argument,
    // ARG1=EVENT_ACK (0 here — plain status read). No ARG2+ exists.
    if (auto cmd = writeCommand(Command::DabGetEventStatus, nullptr, 0U);
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    std::array<std::uint8_t, 9> raw = {};
    if (auto rd = readRaw(raw); !rd) {
        return std::unexpected(rd.error());
    }

    // raw[5]=RESP4 (see readFmRds()); SVRLISTINT is RESP4 bit0.
    Si4684DabEventStatus events = {};
    events.serviceListReady = (raw[5] & 0x01U) != 0U;
    events.reconfig = (raw[5] & 0x02U) != 0U;
    return events;
}

std::expected<std::vector<Si4684DabService>, Si4684Error>
Si4684Driver::fetchDabServiceList()
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return std::unexpected(band.error());
    }

    // AN649 Command 0x80 GET_DIGITAL_SERVICE_LIST has a single argument,
    // ARG1=SERTYPE (0 = complete DAB/DMB service list). No ARG2+ exists.
    if (auto cmd = writeCommand(Command::GetDigitalServiceList, nullptr, 0U);
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }

    std::array<std::uint8_t, 9> header = {};
    if (auto rd = readRaw(header); !rd) {
        return std::unexpected(rd.error());
    }

    // raw[5]=RESP4=SIZE[7:0], raw[6]=RESP5=SIZE[15:8] (see readFmRds()).
    // AN649 only documents SIZE/DATA_0/DATA_N generically for this command
    // and defers the DAB payload layout to a supplemental "Digital
    // Services User's Guide" we don't have; the exact field layout below
    // is cross-checked against hitech95/si468x_dab_receiver's
    // si468x_core_cmd_dab_get_service_list() (drivers/mfd/si468x-cmd.c),
    // a real working Linux driver for the same command. That driver also
    // establishes that the payload actually carried after SIZE is
    // SIZE-2 bytes, not SIZE bytes.
    const std::uint16_t payloadSize = readLe16(header.data() + 5);
    if (payloadSize <= 2U || payloadSize + 5U > kSpiBufferSize) {
        return std::unexpected(Si4684Error::ReplyTooShort);
    }

    // DATA_0 (first byte of the payload) is RESP6 = body[7]: lead-in(1) +
    // STATUS0-3(4) + SIZE(2) = 7 header bytes before it. Total frame is
    // lead-in(1) + STATUS0-3(4) + SIZE(2) + payload(SIZE-2) = SIZE+5.
    std::vector<std::uint8_t> body(payloadSize + 5U, 0U);
    if (auto rd = readRaw(body); !rd) {
        return std::unexpected(rd.error());
    }

    // From DATA_0 (body[7]): Version(2) + NumServices/flags(1) +
    // AlignPad(3) = 6 bytes, then Service 1 begins at body[13]. (The
    // previous version of this code double-counted the already-consumed
    // SIZE field here, offsetting every read by 2 bytes — that is why the
    // service list always came back empty.)
    const std::uint8_t serviceCount = body[9] & 0x1FU; // max 32 services
    std::vector<Si4684DabService> services;
    services.reserve(serviceCount);

    std::size_t offset = 13U;
    for (std::uint8_t i = 0; i < serviceCount; ++i) {
        // Fixed per-service part: ServiceID(4) + ServiceInfo1-3(3) +
        // AlignPad(1) + Label(16) = 24 bytes.
        if (offset + 24U > body.size()) {
            break;
        }
        Si4684DabService entry = {};
        entry.serviceId = readLe32(body.data() + offset);
        entry.serviceType = body[offset + 4U];
        const std::uint8_t componentCount = body[offset + 5U] & 0x0FU;
        std::memcpy(entry.label.data(), body.data() + offset + 8U, 16U);
        entry.label[16] = '\0';
        offset += 24U;

        // Component ID is 2 bytes (hitech95's si468x-cmd.c packs tm_id/
        // sub_ch_id/fidc_id/sc_id into this same field; only the raw
        // 16-bit value is exposed on this DTO). Only the first
        // component's ID is exposed on this DTO. Every component (M =
        // componentCount) must still be skipped to keep the next service
        // entry aligned, each one 2 bytes packed field + ServiceType/
        // flags(1) + ValidFlags(1) = 4 bytes.
        if (componentCount > 0U && offset + 2U <= body.size()) {
            entry.componentId = readLe16(body.data() + offset);
        }
        offset += static_cast<std::size_t>(componentCount) * 4U;
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
    // AN649 Command 0x81 START_DIGITAL_SERVICE: ARG1=SERTYPE, ARG2-3=0x00
    // fixed, ARG4-7=SERVICE_ID (LE32), ARG8-11=COMP_ID (LE32). writeCommand()
    // supplies ARG1 (=type), so this array starts at ARG2.
    const std::uint8_t args[] = {
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
    if (auto cmd = writeCommand(Command::StartDigitalService, args,
                                sizeof(args), static_cast<std::uint8_t>(type));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    return {};
}

std::expected<void, Si4684Error> Si4684Driver::stopDabService(
    std::uint32_t serviceId,
    std::uint32_t componentId,
    Si4684DigitalServiceType type)
{
    if (auto band = ensureBand(Si4684Band::Dab); !band) {
        return band;
    }
    // AN649 Command 0x82 STOP_DIGITAL_SERVICE: same layout as
    // START_DIGITAL_SERVICE (ARG1=SERTYPE, ARG2-3=0x00 fixed, ARG4-7=
    // SERVICE_ID LE32, ARG8-11=COMP_ID LE32).
    const std::uint8_t args[] = {
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
    if (auto cmd = writeCommand(Command::StopDigitalService, args,
                                sizeof(args), static_cast<std::uint8_t>(type));
        !cmd) {
        return std::unexpected(Si4684Error::CommandFailed);
    }
    return {};
}

} // namespace si4684
