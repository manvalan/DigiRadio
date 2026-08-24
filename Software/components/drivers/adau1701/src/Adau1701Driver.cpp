/**
 * @file    Adau1701Driver.cpp
 * @brief   Adau1701Driver implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "adau1701/Adau1701Driver.hpp"

#include "adau1701/Adau1701ParamMap.hpp"

#include "core/BiquadDesign.hpp"
#include "core/DspProgram.hpp"

#include "DigiRadio_IC_1_PARAM.h"
#include "SigmaStudioFW.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C"
{

} // extern "C"

namespace adau1701
{

    namespace
    {
        constexpr char kTag[] = "Adau1701";
        constexpr int kI2cPort = 0;
        /** Index 0 is the fixed high-pass band (SigmaStudio band 1); not safeloaded. */
        constexpr std::uint8_t kFixedHighPassBandIndex = 0U;
    } // namespace

    Adau1701Driver::Adau1701Driver(Adau1701Pins pins,
                                   core::IDspProgramSource &programSource)
        : pins_(pins), programSource_(programSource), booted_(false), i2cBus_(nullptr), i2cDev_(nullptr)
    {
    }

    Adau1701Driver::~Adau1701Driver()
    {
        auto *dev = static_cast<i2c_master_dev_handle_t>(i2cDev_);
        auto *bus = static_cast<i2c_master_bus_handle_t>(i2cBus_);
        if (dev != nullptr)
        {
            i2c_master_bus_rm_device(dev);
        }
        if (bus != nullptr)
        {
            i2c_del_master_bus(bus);
        }
    }

    std::expected<void, Adau1701Error> Adau1701Driver::replayProgram(
        const core::DspProgram &program)
    {
        const unsigned char deviceAddr =
            static_cast<unsigned char>(pins_.i2cAddr7 << 1);
        std::size_t index = 0U;
        for (const core::RegisterWrite &write : program.writes())
        {
            const auto data = write.data();
            if (data.empty())
            {
                return std::unexpected(Adau1701Error::DownloadFailed);
            }
            auto *bytes = const_cast<ADI_REG_TYPE *>(
                reinterpret_cast<const ADI_REG_TYPE *>(data.data()));
            const auto length = static_cast<unsigned int>(data.size());
            if (SIGMA_WRITE_REGISTER_BLOCK(deviceAddr, write.address(), length,
                                           bytes) != 0)
            {
                ESP_LOGE(kTag,
                         "program write #%u failed (addr=0x%04X len=%u) "
                         "after retries",
                         static_cast<unsigned>(index),
                         static_cast<unsigned>(write.address()),
                         static_cast<unsigned>(length));
                return std::unexpected(Adau1701Error::DownloadFailed);
            }
            // Diagnostic only: an ACKed write that still reads back wrong
            // would otherwise be invisible (see the SIGMA_WRITE_REGISTER_BLOCK
            // comment in SigmaStudioFW.c). Logged, not fatal -- some
            // addresses in this range may be self-clearing/status bits
            // that legitimately don't read back what was written.
            if (sigma_verify_block(write.address(), bytes, length) != 0)
            {
                ESP_LOGW(kTag,
                         "program write #%u read-back mismatch (addr=0x%04X "
                         "len=%u) -- ACKed but did not land as written",
                         static_cast<unsigned>(index),
                         static_cast<unsigned>(write.address()),
                         static_cast<unsigned>(length));
            }
            ++index;
        }
        return {};
    }

    std::expected<void, Adau1701Error> Adau1701Driver::boot()
    {
        if (booted_)
        {
            return {};
        }

        gpio_config_t resetCfg = {};
        resetCfg.pin_bit_mask = 1ULL << pins_.resetGpio;
        resetCfg.mode = GPIO_MODE_OUTPUT;
        if (gpio_config(&resetCfg) != ESP_OK)
        {
            return std::unexpected(Adau1701Error::ResetFailed);
        }

        gpio_set_level(static_cast<gpio_num_t>(pins_.resetGpio), 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(pins_.resetGpio), 1);
        vTaskDelay(pdMS_TO_TICKS(10));

        i2c_master_bus_config_t busCfg = {};
        busCfg.i2c_port = static_cast<i2c_port_num_t>(kI2cPort);
        busCfg.sda_io_num = static_cast<gpio_num_t>(pins_.i2cSda);
        busCfg.scl_io_num = static_cast<gpio_num_t>(pins_.i2cScl);
        busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
        busCfg.glitch_ignore_cnt = 7;
        busCfg.flags.enable_internal_pullup = true;

        i2c_master_bus_handle_t bus = nullptr;
        if (i2c_new_master_bus(&busCfg, &bus) != ESP_OK)
        {
            ESP_LOGE(kTag, "i2c_new_master_bus failed");
            return std::unexpected(Adau1701Error::I2cInitFailed);
        }
        i2cBus_ = bus;

        i2c_device_config_t devCfg = {};
        devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        devCfg.device_address = static_cast<uint16_t>(pins_.i2cAddr7);
        devCfg.scl_speed_hz = 100000;

        i2c_master_dev_handle_t dev = nullptr;
        if (i2c_master_bus_add_device(bus, &devCfg, &dev) != ESP_OK)
        {
            ESP_LOGE(kTag, "i2c_master_bus_add_device failed");
            return std::unexpected(Adau1701Error::I2cInitFailed);
        }
        i2cDev_ = dev;

        sigma_studio_bind_i2c(kI2cPort, static_cast<unsigned char>(pins_.i2cAddr7));
        sigma_studio_set_device(dev);

        const auto program = programSource_.loadProgram();
        if (!program)
        {
            ESP_LOGE(kTag, "DSP program load failed");
            return std::unexpected(Adau1701Error::DownloadFailed);
        }
        ESP_LOGI(kTag,
                 "DSP program contains %u writes",
                 static_cast<unsigned>(program->writes().size()));
        for (const auto &w : program->writes())
        {
            ESP_LOGI(kTag,
                     "ADDR=0x%04X LEN=%u",
                     static_cast<unsigned>(w.address()),
                     static_cast<unsigned>(w.data().size()));
        }
        if (auto replay = replayProgram(*program); !replay)
        {
            return replay;
        }

        // Diagnostic (2026-08-24, extended from band-0-only): log EVERY EQ
        // band's live Param RAM contents (bands 0-5, 5 coefficients each --
        // B0,B1,B2,A0,A1 per paramAddrEqBandBase's 5-word stride). Band 0
        // is the fixed high-pass that applyEq() always skips (whatever
        // landed at program-load time plays permanently); bands 1-5 are
        // runtime-safeloaded whenever the audio profile has a nonzero
        // gain, but with the factory-default flat profile (all gains 0)
        // they should read back as the identity biquad (B0=0x00800000=1.0,
        // rest 0) written by designFlatEq(). A single steady test tone at
        // one frequency cannot reveal a bad coefficient elsewhere in a
        // band's response curve -- reading the actual RAM contents is the
        // only way to confirm what's really there, not what the software
        // believes it wrote. Decode as 5.23 (28-bit, matches the ADAU1701's
        // real Param RAM format, NOT a naive 32-bit Q8.23 -- see the
        // 2026-08-23 band-0 false alarm in
        // docs/si4684-rf-investigation-report.md for why that distinction
        // matters).
        for (unsigned band = 0U; band < 6U; ++band)
        {
            const unsigned baseAddr = paramAddrEqBandBase(
                static_cast<std::uint8_t>(band));
            for (unsigned i = 0U; i < 5U; ++i)
            {
                unsigned char raw[4U] = {0U, 0U, 0U, 0U};
                if (sigma_i2c_read(baseAddr + i, raw, sizeof(raw)) == 0)
                {
                    std::int32_t fixpoint =
                        static_cast<std::int32_t>(
                            (static_cast<std::uint32_t>(raw[0]) << 24) |
                            (static_cast<std::uint32_t>(raw[1]) << 16) |
                            (static_cast<std::uint32_t>(raw[2]) << 8) |
                            static_cast<std::uint32_t>(raw[3]));
                    // Sign-extend from bit 27 (28-bit/5.23 format).
                    if ((fixpoint & (1 << 27)) != 0)
                    {
                        fixpoint -= (1 << 28);
                    }
                    ESP_LOGI(kTag,
                             "EQ band%u param[%u] addr=0x%04X raw=0x%08X "
                             "value=%f",
                             band, i, baseAddr + i,
                             static_cast<unsigned>(
                                 (static_cast<std::uint32_t>(raw[0]) << 24)
                                 | (static_cast<std::uint32_t>(raw[1]) << 16)
                                 | (static_cast<std::uint32_t>(raw[2]) << 8)
                                 | static_cast<std::uint32_t>(raw[3])),
                             static_cast<double>(fixpoint) /
                                 static_cast<double>(1U << 23));
                }
                else
                {
                    ESP_LOGW(kTag, "EQ band%u param[%u] read-back failed",
                             band, i);
                }
            }
        }

        // SerialInputRegister (0x081F) IBP override -- REMOVED 2026-08-24,
        // CONFIRMED LIVE as the root cause of the multi-day hiss/
        // unintelligible-speech investigation. History: on 2026-08-16
        // (commit 6974095) this was set to IBP=1 (0x08) alongside a
        // SEPARATE, simultaneous fix to Si4684's PIN_CONFIG_ENABLE (which
        // had been forcing the chip's analog DAC fallback instead of real
        // I2S output). Both fixes landed in the same commit and were
        // tested together: "static" became "music", credited to IBP=1.
        // But SerialInputRegister is a SINGLE register shared by every
        // SDATA_INx pin on the ADAU1701 (confirmed against the datasheet
        // 2026-08-24: one INPUT_BCLK/INPUT_LRCLK clock pair serves all
        // four SDATA_INx pins) -- so the override also applied to the
        // ESP32 leg, not just Si4684's. The PIN_CONFIG_ENABLE fix alone
        // was what turned static into music (Si4684 finally sending valid
        // I2S data at all); IBP=1 had never been validated in isolation
        // and was in fact marginal/wrong for both legs. With IBP left at
        // the compiled default (0x00, IBP=0) and PIN_CONFIG_ENABLE
        // independently correct, live listening confirmed clean audio on
        // both the Si4684 (DAB) and ESP32 (web radio) paths -- the hiss
        // is gone. Left commented out below rather than deleted, in case
        // a future hardware revision needs it revisited.
        //
        // {
        //     const unsigned char deviceAddr =
        //         static_cast<unsigned char>(pins_.i2cAddr7 << 1);
        //     ADI_REG_TYPE serialInFix = 0x08U;
        //     if (SIGMA_WRITE_REGISTER_BLOCK(deviceAddr, 0x081FU, 1U,
        //                                    &serialInFix) != 0)
        //     {
        //         ESP_LOGE(kTag, "SerialInputRegister override failed after retries");
        //         return std::unexpected(Adau1701Error::DownloadFailed);
        //     }
        //     if (sigma_verify_block(0x081FU, &serialInFix, 1U) != 0)
        //     {
        //         ESP_LOGW(kTag,
        //                  "SerialInputRegister read-back mismatch -- ACKed "
        //                  "but did not land as 0x08");
        //     }
        // }

        // Limiter1/Limiter2 threshold override, 2026-08-24: compiled
        // program ships both at 0x00800000 = 1.0 linear = 0 dBFS (an
        // RMS-detecting limiter, per Analog Devices' own SigmaStudio
        // Limiter cell docs), with zero headroom anywhere upstream (every
        // mixer/EQ/master-volume gain in the compiled program is unity).
        // A quiet synthetic test tone (well under 0 dBFS RMS) never
        // engaged it and sounded clean; real loudness-normalized FM/DAB/
        // streamed program content sits close to 0 dBFS RMS routinely,
        // triggering continuous gain-reduction ("pumping" per ADI's own
        // docs) heard as exactly the hiss/unintelligible-speech symptom
        // under investigation. Pulling both thresholds down to -6 dBFS
        // gives real margin without being so conservative it can't be
        // heard whether this was the mechanism.
        {
            constexpr float kLimiterThresholdDb = -6.0F;
            constexpr float kLimiterThresholdLinear = 0.50118723F; // 10^(-6/20)
            const std::int32_t thresholdFixpoint =
                core::floatToFixpoint823(kLimiterThresholdLinear);
            if (auto lim1 = safeloadFixpoint(
                    static_cast<unsigned>(ADDR_LIMITER1_THRESHOLD),
                    thresholdFixpoint);
                !lim1)
            {
                ESP_LOGW(kTag, "Limiter1 threshold override failed");
            }
            if (auto lim2 = safeloadFixpoint(
                    static_cast<unsigned>(ADDR_LIMITER2_THRESHOLD),
                    thresholdFixpoint);
                !lim2)
            {
                ESP_LOGW(kTag, "Limiter2 threshold override failed");
            }
            ESP_LOGI(kTag, "Limiter1/2 threshold set to %.1f dBFS",
                     static_cast<double>(kLimiterThresholdDb));
        }

        booted_ = true;
        ESP_LOGI(kTag, "SigmaStudio program loaded");
        return {};
    }

    bool Adau1701Driver::isBooted() const noexcept
    {
        return booted_;
    }

    void *Adau1701Driver::i2cBusHandle() const noexcept
    {
        return i2cBus_;
    }

    std::expected<void, Adau1701Error> Adau1701Driver::ensureBooted() const
    {
        if (!booted_)
        {
            return std::unexpected(Adau1701Error::NotBooted);
        }
        return {};
    }

    std::expected<void, Adau1701Error> Adau1701Driver::safeloadFixpoint(
        unsigned paramAddr, std::int32_t fixpoint)
    {
        sigma_studio_lock();
        const int result = sigma_safeload_param(paramAddr, fixpoint);
        sigma_studio_unlock();
        if (result != 0)
        {
            return std::unexpected(Adau1701Error::SafeloadFailed);
        }
        return {};
    }

    std::expected<void, Adau1701Error> Adau1701Driver::safeloadGain(
        unsigned paramAddr, core::GainDb gain)
    {
        return safeloadFixpoint(paramAddr, core::gainDbToLinearFixpoint(gain));
    }

    std::expected<void, Adau1701Error> Adau1701Driver::setInputVolume(
        core::MixSource source, core::GainDb left, core::GainDb right)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }
        if (auto result = safeloadGain(paramAddrInputLeft(source), left); !result)
        {
            return result;
        }
        return safeloadGain(paramAddrInputRight(source), right);
    }

    std::expected<void, Adau1701Error> Adau1701Driver::setMasterVolume(
        core::GainDb left, core::GainDb right)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }
        if (auto result = safeloadGain(static_cast<unsigned>(ADDR_MULTIPLE1), left);
            !result)
        {
            return result;
        }
        return safeloadGain(static_cast<unsigned>(ADDR_MULTIPLE1_1), right);
    }

    std::expected<void, Adau1701Error> Adau1701Driver::applyMixer(
        const core::MixerState &mixer)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }
        if (auto result = setInputVolume(core::MixSource::Si4684, mixer.si4684Left,
                                         mixer.si4684Right);
            !result)
        {
            ESP_LOGW(kTag, "applyMixer: si4684 input safeload failed");
            return result;
        }
        if (auto result = setInputVolume(core::MixSource::Esp32, mixer.esp32Left,
                                         mixer.esp32Right);
            !result)
        {
            ESP_LOGW(kTag, "applyMixer: esp32 input safeload failed");
            return result;
        }
        if (auto result =
                safeloadGain(static_cast<unsigned>(ADDR_STMIXER1_ST0_VOLUME),
                             mixer.mixLeft);
            !result)
        {
            ESP_LOGW(kTag, "applyMixer: st0 safeload failed");
            return result;
        }
        if (auto result = safeloadGain(
                static_cast<unsigned>(ADDR_STMIXER1_ST1_VOLUME), mixer.mixRight);
            !result)
        {
            ESP_LOGW(kTag, "applyMixer: st1 safeload failed");
            return result;
        }
        return {};
    }

    std::expected<void, Adau1701Error> Adau1701Driver::setEqBand(
        core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center, float q)
    {
        if (band.value() == kFixedHighPassBandIndex)
        {
            return std::unexpected(Adau1701Error::InvalidParameter);
        }

        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }

        const core::BiquadCoefficients coeffs =
            core::designPeakingEq(center, gain, q);
        const auto fixpoints = coeffs.toFixpoint823();
        const unsigned baseAddr = paramAddrEqBandBase(band.value());

        unsigned addrs[5U];
        int values[5U];
        for (unsigned i = 0U; i < 5U; ++i)
        {
            addrs[i] = baseAddr + i;
            values[i] = fixpoints[i];
        }

        sigma_studio_lock();
        const int result = sigma_safeload_block(5U, addrs, values);
        sigma_studio_unlock();
        if (result != 0)
        {
            return std::unexpected(Adau1701Error::SafeloadFailed);
        }
        return {};
    }

    std::expected<void, Adau1701Error> Adau1701Driver::setBeepEnabled(
        bool enabled)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }
        // Beep1 ("Beep - variable gain", ADI Sound Generation toolbox,
        // DigiRadio.params) ENABLE/KICK: unity fixpoint 0x00800000 = on,
        // exact zero = off. Not continuous gains, so bypass
        // safeloadGain/GainDb (whose quietest value is -96 dB, not true
        // zero) and write the raw fixpoint. KICK is the cell's trigger
        // input (per its own SigmaStudio parameter name); ENABLE alone
        // may leave the generator gated shut without it.
        const std::int32_t value =
            enabled ? core::gainDbToLinearFixpoint(core::GainDb::zero()) : 0;
        if (auto result = safeloadFixpoint(
                static_cast<unsigned>(ADDR_BEEP1_ENABLE), value);
            !result)
        {
            return result;
        }
        return safeloadFixpoint(static_cast<unsigned>(ADDR_BEEP1_KICK), value);
    }

    std::expected<void, Adau1701Error> Adau1701Driver::writeRawParam(
        unsigned address, float value)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }
        return safeloadFixpoint(address, core::floatToFixpoint823(value));
    }

    std::expected<void, Adau1701Error> Adau1701Driver::applyEq(
        const core::EqProfile &eq)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }

        for (std::uint8_t i = 0; i < core::EqBandIndex::kBandCount; ++i)
        {
            if (i == kFixedHighPassBandIndex)
            {
                continue;
            }
            const auto index = core::EqBandIndex::tryFromIndex(i);
            if (!index)
            {
                return std::unexpected(Adau1701Error::SafeloadFailed);
            }
            const core::EqBandSettings &band = eq.band(*index);
            if (auto result = setEqBand(*index, band.gain, band.center, band.q);
                !result)
            {
                ESP_LOGW(kTag, "applyEq: band %u safeload failed",
                        static_cast<unsigned>(i));
                return result;
            }
        }
        return {};
    }

    std::expected<void, Adau1701Error> Adau1701Driver::applyProfile(
        const core::AudioProfile &profile)
    {
        if (auto ready = ensureBooted(); !ready)
        {
            return ready;
        }
        if (auto result = applyMixer(profile.mixer); !result)
        {
            return result;
        }
        if (auto result = applyEq(profile.eq); !result)
        {
            return result;
        }
        return setMasterVolume(profile.masterLeft, profile.masterRight);
    }

} // namespace adau1701
