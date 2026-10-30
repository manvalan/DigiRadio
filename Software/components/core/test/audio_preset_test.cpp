/**
 * @file    audio_preset_test.cpp
 * @brief   Host tests for audio presets: ids, bank, NVS/EEPROM codecs, JSON.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/AudioPreset.hpp"
#include "core/AudioPresetBank.hpp"
#include "core/AudioPresetCodec.hpp"
#include "core/AudioPresetJson.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++gFailures;
    }
}

[[nodiscard]] core::AudioPreset makePreset(std::string_view name, float gain)
{
    core::EqProfile eq = core::EqProfile::factoryDefault();
    for (std::uint8_t i = 0; i < core::EqBandIndex::kBandCount; ++i) {
        const auto index = *core::EqBandIndex::tryFromIndex(i);
        auto band = eq.band(index);
        band.gain = *core::GainDb::tryFromDb(gain + static_cast<float>(i));
        band.q = 1.5F;
        eq.setBand(index, band);
    }
    return core::AudioPreset{
        .name = *core::PresetName::tryFrom(name),
        .eq = eq,
        .enhancements = core::AudioEnhancements{
            .stereo = *core::EnhanceLevel::tryFromLevel(25),
            .bass = *core::EnhanceLevel::tryFromLevel(70),
        },
    };
}

[[nodiscard]] float bandGain(const core::AudioPreset& p, std::uint8_t band)
{
    return p.eq.band(*core::EqBandIndex::tryFromIndex(band)).gain.value();
}

void testIds()
{
    check(core::PresetId::tryFromValue(0).has_value(), "id 0 valid");
    check(core::PresetId::tryFromValue(5).has_value(), "id 5 valid");
    check(!core::PresetId::tryFromValue(6).has_value(), "id 6 invalid");
    check(!core::PresetId::tryFromValue(9).has_value(), "id 9 invalid");
    check(core::PresetId::tryFromValue(19).has_value(), "id 19 valid");
    check(!core::PresetId::tryFromValue(20).has_value(), "id 20 invalid");
    const auto user = core::PresetId::user(3);
    check(user.value() == 13 && !user.isFactory() && user.index() == 3,
          "user(3) is id 13");
}

void testNames()
{
    check(core::PresetName::tryFrom("Jazz sera").has_value(), "plain name");
    check(!core::PresetName::tryFrom("").has_value(), "empty rejected");
    check(!core::PresetName::tryFrom("0123456789abcdef").has_value(),
          "16 bytes rejected");
    check(!core::PresetName::tryFrom("a\"b").has_value(), "quote rejected");
    check(!core::PresetName::tryFrom("a\nb").has_value(), "newline rejected");
}

void testFactory()
{
    const auto presets = core::factoryPresets();
    check(presets.size() == 6U, "six factory presets");
    check(presets[0].name.value() == "Neutro", "first is Neutro");
    check(presets[5].name.value() == "Notte", "last is Notte");
    check(std::fabs(bandGain(presets[1], 1) + 4.0F) < 0.01F,
          "Voce cuts 100 Hz by 4 dB");
}

void testBank()
{
    core::AudioPresetBank bank;
    bank.store(2, makePreset("Mio", 1.0F));
    const auto id = core::PresetId::user(2);
    check(bank.find(id).has_value(), "stored slot found");
    check(!bank.find(core::PresetId::user(3)).has_value(), "empty slot");
    check(bank.find(*core::PresetId::tryFromValue(4)).has_value(),
          "factory found");
    bank.setSourceDefault(core::ActiveSource::Radio, id);
    bank.setLast(id);
    bank.erase(2);
    check(!bank.find(id).has_value(), "erased");
    check(!bank.sourceDefault(core::ActiveSource::Radio).has_value(),
          "erase clears default");
    check(!bank.last().has_value(), "erase clears last");
    bank.setSourceDefault(core::ActiveSource::Beep, id);
    check(!bank.sourceDefault(core::ActiveSource::Beep).has_value(),
          "beep has no default");
}

[[nodiscard]] core::AudioPresetBank sampleBank()
{
    core::AudioPresetBank bank;
    bank.store(0, makePreset("Sera", -2.5F));
    bank.store(5, makePreset("Cucina lunga", 3.0F));
    bank.store(9, makePreset("Ultimo", 0.5F));
    bank.setSourceDefault(core::ActiveSource::Radio,
                          *core::PresetId::tryFromValue(1));
    bank.setSourceDefault(core::ActiveSource::Bluetooth,
                          core::PresetId::user(5));
    bank.setLast(core::PresetId::user(0));
    return bank;
}

void testBlobRoundTrip()
{
    const auto blob = core::encodePresetBankBlob(sampleBank());
    const auto back = core::decodePresetBankBlob(blob);
    check(back.has_value(), "blob decodes");
    if (!back) {
        return;
    }
    const auto slot5 = back->userSlot(5);
    check(slot5 && slot5->name.value() == "Cucina lunga", "blob keeps name");
    check(slot5 && std::fabs(bandGain(*slot5, 2) - 5.0F) < 0.01F,
          "blob keeps gain");
    check(slot5 && std::fabs(slot5->eq.bands()[0].q - 1.5F) < 0.01F,
          "blob keeps q");
    check(back->userSlot(9).has_value(), "blob keeps slot 9");
    check(back->last() == core::PresetId::user(0), "blob keeps last");
    check(back->sourceDefault(core::ActiveSource::Bluetooth)
              == core::PresetId::user(5),
          "blob keeps stream default");
    auto broken = blob;
    broken[2] = 9U;
    check(!core::decodePresetBankBlob(broken).has_value(), "bad version");
    broken = blob;
    broken.resize(blob.size() - 3U);
    check(!core::decodePresetBankBlob(broken).has_value(), "truncated");
}

void testEepromRoundTrip()
{
    const auto image = core::encodePresetEepromImage(sampleBank());
    const auto back = core::decodePresetEepromImage(image);
    check(back.has_value(), "eeprom decodes");
    if (!back) {
        return;
    }
    const auto slot5 = back->userSlot(5);
    check(slot5 && slot5->name.value() == "Cucina", "eeprom name 7 chars");
    check(slot5 && std::fabs(bandGain(*slot5, 3) - 6.0F) < 0.01F,
          "eeprom keeps gain");
    check(slot5 && slot5->enhancements.bass.value() == 70U,
          "eeprom keeps bass");
    const auto slot0 = back->userSlot(0);
    check(slot0 && std::fabs(bandGain(*slot0, 0) + 2.5F) < 0.01F,
          "eeprom half-dB");
    check(!back->userSlot(9).has_value(), "slot 9 not mirrored");
    check(back->sourceDefault(core::ActiveSource::Radio)
              == *core::PresetId::tryFromValue(1),
          "eeprom keeps radio default");

    auto torn = image;
    torn[16U * 6U + 3U] ^= 0x55U; // slot 5 page
    const auto partial = core::decodePresetEepromImage(torn);
    check(partial && !partial->userSlot(5) && partial->userSlot(0),
          "bad page CRC only drops that slot");

    core::PresetEepromImage blank{};
    blank.fill(0xFFU);
    check(!core::decodePresetEepromImage(blank).has_value(), "blank image");
}

void testJson()
{
    const std::string list = core::serializePresetListJson(sampleBank());
    check(list.find("\"name\":\"Neutro\"") != std::string::npos,
          "list has factory");
    check(list.find("{\"slot\":5,\"id\":15,\"name\":\"Cucina lunga\"")
              != std::string::npos,
          "list has user slot");
    check(list.find("\"defaults\":{\"radio\":1,\"stream\":15}")
              != std::string::npos,
          "list has defaults");
    check(list.find("\"last\":10}") != std::string::npos, "list has last");

    check(core::parsePresetIdJson("{\"id\": 12}")->value() == 12U, "id");
    check(!core::parsePresetIdJson("{\"id\":7}").has_value(), "bad id");
    check(*core::parsePresetSlotJson("{\"slot\":9}") == 9U, "slot");
    check(!core::parsePresetSlotJson("{\"slot\":10}").has_value(), "bad slot");

    const auto save = core::parsePresetSaveJson(
        "{\"slot\":1,\"name\":\"Jazz \\\"club\\\"\"}");
    check(!save.has_value(), "quote in name rejected");
    const auto ok = core::parsePresetSaveJson("{\"slot\":1,\"name\":\"Jazz\"}");
    check(ok && ok->slot == 1U && ok->name.value() == "Jazz", "save body");

    const auto defs =
        core::parsePresetDefaultsJson("{\"radio\":null,\"stream\":3}");
    check(defs && !defs->radio && defs->stream
              && defs->stream->value() == 3U,
          "defaults body");
    check(!core::parsePresetDefaultsJson("{\"radio\":3}").has_value(),
          "defaults need both keys");
}

} // namespace

int main()
{
    testIds();
    testNames();
    testFactory();
    testBank();
    testBlobRoundTrip();
    testEepromRoundTrip();
    testJson();
    if (gFailures != 0) {
        std::cerr << gFailures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "audio_preset_test: all passed\n";
    return EXIT_SUCCESS;
}
