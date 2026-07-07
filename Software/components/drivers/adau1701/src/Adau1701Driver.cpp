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

extern "C" {

} // extern "C"

namespace adau1701 {

namespace {
constexpr char kTag[] = "Adau1701";
constexpr int kI2cPort = 0;
/** Index 0 is the fixed high-pass band (SigmaStudio band 1); not safeloaded. */
constexpr std::uint8_t kFixedHighPassBandIndex = 0U;
} // namespace

Adau1701Driver::Adau1701Driver(Adau1701Pins pins,
                               core::IDspProgramSource& programSource)
    : pins_(pins)
    , programSource_(programSource)
    , booted_(false)
    , i2cBus_(nullptr)
    , i2cDev_(nullptr)
{
}

Adau1701Driver::~Adau1701Driver()
{
    auto* dev = static_cast<i2c_master_dev_handle_t>(i2cDev_);
    auto* bus = static_cast<i2c_master_bus_handle_t>(i2cBus_);
    if (dev != nullptr) {
        i2c_master_bus_rm_device(dev);
    }
    if (bus != nullptr) {
        i2c_del_master_bus(bus);
    }
}

std::expected<void, Adau1701Error> Adau1701Driver::replayProgram(
    const core::DspProgram& program)
{
    const unsigned char deviceAddr =
        static_cast<unsigned char>(pins_.i2cAddr7 << 1);
    for (const core::RegisterWrite& write : program.writes()) {
        const auto data = write.data();
        if (data.empty()) {
            return std::unexpected(Adau1701Error::DownloadFailed);
        }
        SIGMA_WRITE_REGISTER_BLOCK(
            deviceAddr,
            write.address(),
            static_cast<unsigned int>(data.size()),
            const_cast<ADI_REG_TYPE*>(
                reinterpret_cast<const ADI_REG_TYPE*>(data.data())));
    }
    return {};
}

std::expected<void, Adau1701Error> Adau1701Driver::boot()
{
    if (booted_) {
        return {};
    }

    gpio_config_t resetCfg = {};
    resetCfg.pin_bit_mask = 1ULL << pins_.resetGpio;
    resetCfg.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&resetCfg) != ESP_OK) {
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
    if (i2c_new_master_bus(&busCfg, &bus) != ESP_OK) {
        ESP_LOGE(kTag, "i2c_new_master_bus failed");
        return std::unexpected(Adau1701Error::I2cInitFailed);
    }
    i2cBus_ = bus;

    i2c_device_config_t devCfg = {};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = static_cast<uint16_t>(pins_.i2cAddr7);
    devCfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus, &devCfg, &dev) != ESP_OK) {
        ESP_LOGE(kTag, "i2c_master_bus_add_device failed");
        return std::unexpected(Adau1701Error::I2cInitFailed);
    }
    i2cDev_ = dev;

    sigma_studio_bind_i2c(kI2cPort, static_cast<unsigned char>(pins_.i2cAddr7));
    sigma_studio_set_device(dev);

    const auto program = programSource_.loadProgram();
    if (!program) {
        ESP_LOGE(kTag, "DSP program load failed");
        return std::unexpected(Adau1701Error::DownloadFailed);
    }
    if (auto replay = replayProgram(*program); !replay) {
        return replay;
    }

    booted_ = true;
    ESP_LOGI(kTag, "SigmaStudio program loaded");
    return {};
}

bool Adau1701Driver::isBooted() const noexcept
{
    return booted_;
}

void* Adau1701Driver::i2cBusHandle() const noexcept
{
    return i2cBus_;
}

std::expected<void, Adau1701Error> Adau1701Driver::ensureBooted() const
{
    if (!booted_) {
        return std::unexpected(Adau1701Error::NotBooted);
    }
    return {};
}

std::expected<void, Adau1701Error> Adau1701Driver::safeloadFixpoint(
    unsigned paramAddr, std::int32_t fixpoint)
{
    if (sigma_safeload_param(paramAddr, fixpoint) != 0) {
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
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (auto result = safeloadGain(paramAddrInputLeft(source), left); !result) {
        return result;
    }
    return safeloadGain(paramAddrInputRight(source), right);
}

std::expected<void, Adau1701Error> Adau1701Driver::setMasterVolume(
    core::GainDb left, core::GainDb right)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (auto result = safeloadGain(static_cast<unsigned>(ADDR_MULTIPLE1), left);
        !result) {
        return result;
    }
    return safeloadGain(static_cast<unsigned>(ADDR_MULTIPLE1_1), right);
}

std::expected<void, Adau1701Error> Adau1701Driver::applyMixer(
    const core::MixerState& mixer)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (auto result = setInputVolume(core::MixSource::Si4684, mixer.si4684Left,
                                     mixer.si4684Right);
        !result) {
        return result;
    }
    if (auto result = setInputVolume(core::MixSource::Esp32, mixer.esp32Left,
                                     mixer.esp32Right);
        !result) {
        return result;
    }
    if (auto result =
            safeloadGain(static_cast<unsigned>(ADDR_STMIXER1_ST0_VOLUME),
                         mixer.mixLeft);
        !result) {
        return result;
    }
    return safeloadGain(static_cast<unsigned>(ADDR_STMIXER1_ST1_VOLUME),
                        mixer.mixRight);
}

std::expected<void, Adau1701Error> Adau1701Driver::setEqBand(
    core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center, float q)
{
    if (band.value() == kFixedHighPassBandIndex) {
        return std::unexpected(Adau1701Error::InvalidParameter);
    }

    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }

    const core::BiquadCoefficients coeffs =
        core::designPeakingEq(center, gain, q);
    const auto fixpoints = coeffs.toFixpoint823();
    const unsigned baseAddr = paramAddrEqBandBase(band.value());

    unsigned addrs[5U];
    int values[5U];
    for (unsigned i = 0U; i < 5U; ++i) {
        addrs[i] = baseAddr + i;
        values[i] = fixpoints[i];
    }

    if (sigma_safeload_block(5U, addrs, values) != 0) {
        return std::unexpected(Adau1701Error::SafeloadFailed);
    }
    return {};
}

std::expected<void, Adau1701Error> Adau1701Driver::applyEq(
    const core::EqProfile& eq)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }

    for (std::uint8_t i = 0; i < core::EqBandIndex::kBandCount; ++i) {
        if (i == kFixedHighPassBandIndex) {
            continue;
        }
        const auto index = core::EqBandIndex::tryFromIndex(i);
        if (!index) {
            return std::unexpected(Adau1701Error::SafeloadFailed);
        }
        const core::EqBandSettings& band = eq.band(*index);
        if (auto result = setEqBand(*index, band.gain, band.center, band.q);
            !result) {
            return result;
        }
    }
    return {};
}

std::expected<void, Adau1701Error> Adau1701Driver::applyProfile(
    const core::AudioProfile& profile)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (auto result = applyMixer(profile.mixer); !result) {
        return result;
    }
    if (auto result = applyEq(profile.eq); !result) {
        return result;
    }
    return setMasterVolume(profile.masterLeft, profile.masterRight);
}

} // namespace adau1701
