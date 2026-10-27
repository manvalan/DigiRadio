/**
 * @file    preset_service_test.cpp
 * @brief   Host tests for PresetService and click-free source switching.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "audio/AudioService.hpp"
#include "audio/PresetService.hpp"
#include "core/IDsp.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++gFailures;
    }
}

/** Logs the order of DSP writes that matter for presets and switching. */
class TraceDsp final : public core::IDsp {
public:
    std::vector<float> masterLog;
    std::vector<core::ActiveSource> sourceLog;
    float lastBand0Db{0.0F};
    unsigned lastBass{0};
    unsigned lastStereo{0};

    [[nodiscard]] std::expected<void, core::DspError> applyProfile(
        const core::AudioProfile& profile) override
    {
        lastBand0Db = profile.eq.bands()[0].gain.value();
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> selectSource(
        core::ActiveSource source) override
    {
        sourceLog.push_back(source);
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> applyEq(
        const core::EqProfile& eq) override
    {
        lastBand0Db = eq.bands()[0].gain.value();
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setMasterVolume(
        core::GainDb left, core::GainDb) override
    {
        masterLog.push_back(left.value());
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setEqBand(
        core::EqBandIndex, core::GainDb, core::FrequencyHz, float) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setBeepEnabled(
        bool) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setBassBoostLevel(
        core::EnhanceLevel level) override
    {
        lastBass = level.value();
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setStereoSpreadLevel(
        core::EnhanceLevel level) override
    {
        lastStereo = level.value();
        return {};
    }

    [[nodiscard]] std::expected<core::AudioLevels, core::DspError>
    readLevels() override
    {
        return core::AudioLevels{};
    }

    [[nodiscard]] std::expected<void, core::DspError> writeRawParam(
        unsigned, float) override
    {
        return {};
    }
};

/** In-memory store that counts writes. */
class MemoryStore final : public core::IAudioPresetStore {
public:
    std::optional<core::AudioPresetBank> stored;
    int saves{0};

    [[nodiscard]] std::expected<core::AudioPresetBank, core::StoreError>
    loadBank() const override
    {
        if (!stored) {
            return std::unexpected(core::StoreError::NotFound);
        }
        return *stored;
    }

    [[nodiscard]] std::expected<void, core::StoreError> saveBank(
        const core::AudioPresetBank& bank) override
    {
        stored = bank;
        ++saves;
        return {};
    }
};

void testSourceSwitchFades()
{
    TraceDsp dsp;
    audio::AudioService audio(dsp, nullptr);
    dsp.masterLog.clear();
    check(audio.selectSource(core::ActiveSource::Bluetooth, false).has_value(),
          "select stream");
    check(dsp.sourceLog.size() == 1U, "one MX1 write");
    // Fade down (3 steps), then up (3 steps) + the exact user level.
    check(dsp.masterLog.size() == 7U, "fade has 7 master writes");
    if (dsp.masterLog.size() == 7U) {
        check(dsp.masterLog[2] < dsp.masterLog[0], "fade goes down");
        check(dsp.masterLog[6] > dsp.masterLog[3], "fade comes back");
    }
    dsp.masterLog.clear();
    check(audio.selectSource(core::ActiveSource::Bluetooth, false).has_value(),
          "reselect");
    check(dsp.masterLog.empty(), "same input does not fade");
}

void testApplyAndSave()
{
    TraceDsp dsp;
    audio::AudioService audio(dsp, nullptr);
    MemoryStore nvs;
    MemoryStore eeprom;
    audio::PresetService presets(audio, &nvs, &eeprom);
    check(!presets.load(), "nothing stored yet");

    const auto voce = *core::PresetId::tryFromValue(1);
    check(presets.apply(voce).has_value(), "apply factory Voce");
    check(std::fabs(dsp.lastBand0Db - core::factoryPresets()[1]
                                          .eq.bands()[0].gain.value())
              < 0.01F,
          "Voce EQ reached the DSP");
    check(nvs.saves == 1 && eeprom.saves == 0, "apply writes NVS only");
    check(presets.bank().last() == voce, "last recorded");

    check(!presets.apply(core::PresetId::user(4)).has_value(),
          "empty slot refused");

    check(presets.saveCurrent(4, *core::PresetName::tryFrom("Mio")).has_value(),
          "save to slot 4");
    check(eeprom.saves == 1, "save mirrors to EEPROM");
    const auto mine = presets.bank().userSlot(4);
    check(mine && mine->name.value() == "Mio", "slot 4 named");
    check(!presets.saveCurrent(10, *core::PresetName::tryFrom("X")).has_value(),
          "slot 10 refused");

    check(presets.erase(4).has_value(), "erase slot 4");
    check(!presets.bank().userSlot(4).has_value(), "slot 4 empty");
}

void testDefaultsAndFallback()
{
    TraceDsp dsp;
    audio::AudioService audio(dsp, nullptr);
    MemoryStore nvs;
    MemoryStore eeprom;
    {
        audio::PresetService presets(audio, &nvs, &eeprom);
        check(!presets.setDefaults(core::PresetId::user(2), std::nullopt)
                   .has_value(),
              "default to empty slot refused");
        const auto notte = *core::PresetId::tryFromValue(5);
        check(presets.setDefaults(std::nullopt, notte).has_value(),
              "stream default set");
        audio.setSourceListener(&presets);
        check(audio.selectSource(core::ActiveSource::Bluetooth, false)
                  .has_value(),
              "switch to stream");
        check(dsp.lastBass == core::factoryPresets()[5].enhancements.bass.value(),
              "stream default recalled on switch");
        audio.setSourceListener(nullptr);
    }

    nvs.stored.reset(); // NVS erased: EEPROM must restore the bank
    audio::PresetService restored(audio, &nvs, &eeprom);
    check(restored.load(), "restored from backup");
    check(nvs.saves >= 1 && nvs.stored.has_value(), "copied back to NVS");
    check(restored.bank().sourceDefault(core::ActiveSource::Bluetooth)
              == *core::PresetId::tryFromValue(5),
          "default survived");
}

} // namespace

int main()
{
    testSourceSwitchFades();
    testApplyAndSave();
    testDefaultsAndFallback();
    if (gFailures != 0) {
        std::cerr << gFailures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "preset_service_test: all passed\n";
    return EXIT_SUCCESS;
}
