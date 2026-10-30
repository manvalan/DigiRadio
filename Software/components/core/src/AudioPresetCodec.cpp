/**
 * @file    AudioPresetCodec.cpp
 * @brief   NVS blob and 24AA025 image encodings of AudioPresetBank.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/AudioPresetCodec.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace core {

namespace {

constexpr std::uint8_t kMagic0 = 'D';
constexpr std::uint8_t kMagic1 = 'P';
constexpr std::uint8_t kEepromMagic0 = 'A';
constexpr std::uint8_t kEepromMagic1 = 'P';
constexpr std::uint8_t kVersion = 1U;
constexpr std::uint8_t kNoId = 0xFFU;
constexpr std::size_t kPage = 16U;
constexpr std::size_t kEepromNameLen = 7U;

// ---- shared helpers --------------------------------------------------

[[nodiscard]] std::uint8_t encodeId(std::optional<PresetId> id) noexcept
{
    return id ? id->value() : kNoId;
}

[[nodiscard]] std::optional<PresetId> decodeId(std::uint8_t raw) noexcept
{
    if (auto id = PresetId::tryFromValue(raw)) {
        return *id;
    }
    return std::nullopt;
}

void writeHeaderIds(const AudioPresetBank& bank, std::uint8_t* out) noexcept
{
    out[0] = encodeId(bank.last());
    out[1] = encodeId(bank.sourceDefault(ActiveSource::Radio));
    out[2] = encodeId(bank.sourceDefault(ActiveSource::Bluetooth));
}

void readHeaderIds(const std::uint8_t* in, AudioPresetBank& bank) noexcept
{
    bank.setLast(decodeId(in[0]));
    bank.setSourceDefault(ActiveSource::Radio, decodeId(in[1]));
    bank.setSourceDefault(ActiveSource::Bluetooth, decodeId(in[2]));
}

[[nodiscard]] std::optional<AudioEnhancements> makeEnhancements(
    std::uint8_t stereo, std::uint8_t bass) noexcept
{
    const auto s = EnhanceLevel::tryFromLevel(stereo);
    const auto b = EnhanceLevel::tryFromLevel(bass);
    if (!s || !b) {
        return std::nullopt;
    }
    return AudioEnhancements{.stereo = *s, .bass = *b};
}

// ---- NVS blob --------------------------------------------------------

/** Little-endian cursor over the NVS blob. */
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) noexcept
        : data_(data)
    {
    }

    [[nodiscard]] bool u8(std::uint8_t& out) noexcept
    {
        if (pos_ >= data_.size()) {
            return false;
        }
        out = data_[pos_++];
        return true;
    }

    [[nodiscard]] bool u16(std::uint16_t& out) noexcept
    {
        std::uint8_t lo = 0U;
        std::uint8_t hi = 0U;
        if (!u8(lo) || !u8(hi)) {
            return false;
        }
        out = static_cast<std::uint16_t>(lo | (hi << 8));
        return true;
    }

    [[nodiscard]] bool text(std::size_t len, std::string& out)
    {
        if (data_.size() - pos_ < len) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(data_.data() + pos_), len);
        pos_ += len;
        return true;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_{0U};
};

void putU16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void putBand(std::vector<std::uint8_t>& out, const EqBandSettings& band)
{
    const auto gain = static_cast<std::int16_t>(
        std::lround(band.gain.value() * 10.0F));
    putU16(out, static_cast<std::uint16_t>(gain));
    putU16(out, static_cast<std::uint16_t>(band.center.value()));
    putU16(out, static_cast<std::uint16_t>(std::lround(band.q * 1000.0F)));
}

void putPreset(std::vector<std::uint8_t>& out, const AudioPreset& preset)
{
    const std::string_view name = preset.name.value();
    out.push_back(static_cast<std::uint8_t>(name.size()));
    out.insert(out.end(), name.begin(), name.end());
    for (const auto& band : preset.eq.bands()) {
        putBand(out, band);
    }
    out.push_back(preset.enhancements.stereo.value());
    out.push_back(preset.enhancements.bass.value());
}

[[nodiscard]] std::optional<EqBandSettings> readBand(Reader& in)
{
    std::uint16_t gain = 0U;
    std::uint16_t hz = 0U;
    std::uint16_t q = 0U;
    if (!in.u16(gain) || !in.u16(hz) || !in.u16(q)) {
        return std::nullopt;
    }
    const auto g = GainDb::tryFromDb(
        static_cast<float>(static_cast<std::int16_t>(gain)) / 10.0F);
    const auto f = FrequencyHz::tryFromHz(hz);
    if (!g || !f) {
        return std::nullopt;
    }
    return EqBandSettings{
        .gain = *g, .center = *f, .q = static_cast<float>(q) / 1000.0F};
}

[[nodiscard]] std::optional<EqProfile> readEq(Reader& in)
{
    EqProfile eq = EqProfile::factoryDefault();
    for (std::uint8_t i = 0; i < EqBandIndex::kBandCount; ++i) {
        const auto band = readBand(in);
        if (!band) {
            return std::nullopt;
        }
        eq.setBand(*EqBandIndex::tryFromIndex(i), *band);
    }
    return eq;
}

[[nodiscard]] std::optional<AudioPreset> readPreset(Reader& in)
{
    std::uint8_t len = 0U;
    std::string label;
    if (!in.u8(len) || !in.text(len, label)) {
        return std::nullopt;
    }
    const auto name = PresetName::tryFrom(label);
    const auto eq = readEq(in);
    std::uint8_t stereo = 0U;
    std::uint8_t bass = 0U;
    if (!name || !eq || !in.u8(stereo) || !in.u8(bass)) {
        return std::nullopt;
    }
    const auto enh = makeEnhancements(stereo, bass);
    if (!enh) {
        return std::nullopt;
    }
    return AudioPreset{.name = *name, .eq = *eq, .enhancements = *enh};
}

[[nodiscard]] bool readSlot(Reader& in, std::uint8_t slot,
                            AudioPresetBank& bank)
{
    std::uint8_t present = 0U;
    if (!in.u8(present) || present > 1U) {
        return false;
    }
    if (present == 0U) {
        return true;
    }
    auto preset = readPreset(in);
    if (!preset) {
        return false;
    }
    bank.store(slot, std::move(*preset));
    return true;
}

// ---- EEPROM image ----------------------------------------------------

[[nodiscard]] std::uint8_t crc8(const std::uint8_t* data,
                                std::size_t len) noexcept
{
    std::uint8_t crc = 0U;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint8_t>(
                (crc & 0x80U) != 0U ? (crc << 1) ^ 0x07U : crc << 1);
        }
    }
    return crc;
}

void sealPage(std::uint8_t* page) noexcept
{
    page[kPage - 1U] = crc8(page, kPage - 1U);
}

[[nodiscard]] bool pageValid(const std::uint8_t* page) noexcept
{
    return page[kPage - 1U] == crc8(page, kPage - 1U);
}

[[nodiscard]] std::int8_t halfDb(const GainDb& gain) noexcept
{
    const float clamped = std::clamp(gain.value(), -64.0F, 63.5F);
    return static_cast<std::int8_t>(std::lround(clamped * 2.0F));
}

void encodeSlotPage(const AudioPreset& preset, std::uint8_t* page) noexcept
{
    std::fill(page, page + kPage, 0U);
    const std::string_view name = preset.name.value();
    std::copy_n(name.begin(), std::min(name.size(), kEepromNameLen), page);
    const auto& bands = preset.eq.bands();
    for (std::size_t i = 0; i < bands.size(); ++i) {
        page[kEepromNameLen + i] =
            static_cast<std::uint8_t>(halfDb(bands[i].gain));
    }
    page[13] = preset.enhancements.stereo.value();
    page[14] = preset.enhancements.bass.value();
    sealPage(page);
}

[[nodiscard]] std::optional<EqProfile> eqFromHalfDb(const std::uint8_t* gains)
{
    EqProfile eq = EqProfile::factoryDefault();
    for (std::uint8_t i = 0; i < EqBandIndex::kBandCount; ++i) {
        const auto db = static_cast<float>(static_cast<std::int8_t>(gains[i]));
        const auto gain = GainDb::tryFromDb(db / 2.0F);
        if (!gain) {
            return std::nullopt;
        }
        const auto index = *EqBandIndex::tryFromIndex(i);
        EqBandSettings band = eq.band(index);
        band.gain = *gain;
        eq.setBand(index, band);
    }
    return eq;
}

[[nodiscard]] std::optional<AudioPreset> decodeSlotPage(
    const std::uint8_t* page)
{
    if (!pageValid(page)) {
        return std::nullopt;
    }
    const auto* text = reinterpret_cast<const char*>(page);
    std::string_view label(text, std::find(text, text + kEepromNameLen, '\0'));
    label = label.substr(0, label.find_last_not_of(' ') + 1U); // npos+1 == 0
    const auto name = PresetName::tryFrom(label);
    const auto eq = eqFromHalfDb(page + kEepromNameLen);
    const auto enh = makeEnhancements(page[13], page[14]);
    if (!name || !eq || !enh) {
        return std::nullopt;
    }
    return AudioPreset{.name = *name, .eq = *eq, .enhancements = *enh};
}

} // namespace

std::vector<std::uint8_t> encodePresetBankBlob(const AudioPresetBank& bank)
{
    std::vector<std::uint8_t> out{kMagic0, kMagic1, kVersion, 0U, 0U, 0U};
    writeHeaderIds(bank, out.data() + 3);
    for (std::uint8_t slot = 0; slot < PresetId::kUserSlots; ++slot) {
        const auto preset = bank.userSlot(slot);
        out.push_back(preset ? 1U : 0U);
        if (preset) {
            putPreset(out, *preset);
        }
    }
    return out;
}

std::expected<AudioPresetBank, StoreError> decodePresetBankBlob(
    std::span<const std::uint8_t> blob)
{
    if (blob.size() < 6U || blob[0] != kMagic0 || blob[1] != kMagic1
        || blob[2] != kVersion) {
        return std::unexpected(StoreError::InvalidData);
    }
    AudioPresetBank bank;
    Reader in(blob.subspan(6));
    for (std::uint8_t slot = 0; slot < PresetId::kUserSlots; ++slot) {
        if (!readSlot(in, slot, bank)) {
            return std::unexpected(StoreError::InvalidData);
        }
    }
    readHeaderIds(blob.data() + 3, bank);
    return bank;
}

PresetEepromImage encodePresetEepromImage(const AudioPresetBank& bank)
{
    PresetEepromImage image{};
    image.fill(0xFFU);
    image[0] = kEepromMagic0;
    image[1] = kEepromMagic1;
    image[2] = kVersion;
    writeHeaderIds(bank, image.data() + 3);
    sealPage(image.data());
    for (std::uint8_t slot = 0; slot < kPresetEepromSlots; ++slot) {
        if (const auto preset = bank.userSlot(slot)) {
            encodeSlotPage(*preset, image.data() + kPage * (slot + 1U));
        }
    }
    return image;
}

std::expected<AudioPresetBank, StoreError> decodePresetEepromImage(
    const PresetEepromImage& image)
{
    const bool header = image[0] == kEepromMagic0 && image[1] == kEepromMagic1
                        && image[2] == kVersion && pageValid(image.data());
    if (!header) {
        return std::unexpected(StoreError::NotFound);
    }
    AudioPresetBank bank;
    for (std::uint8_t slot = 0; slot < kPresetEepromSlots; ++slot) {
        if (auto preset = decodeSlotPage(image.data() + kPage * (slot + 1U))) {
            bank.store(slot, std::move(*preset));
        }
    }
    readHeaderIds(image.data() + 3, bank);
    return bank;
}

} // namespace core
