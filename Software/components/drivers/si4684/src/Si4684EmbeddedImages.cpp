/**
 * @file    Si4684EmbeddedImages.cpp
 * @brief   Si4684EmbeddedImages implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "si4684/Si4684EmbeddedImages.hpp"

#include <cstddef>
#include <cstdint>

/** @cond linker_symbols */
extern "C" {
extern const std::uint8_t rom_patch_016_bin_start[]
    asm("_binary_rom_patch_016_bin_start");
extern const std::uint8_t rom_patch_016_bin_end[]
    asm("_binary_rom_patch_016_bin_end");
extern const std::uint8_t dab_firmware_bin_start[]
    asm("_binary_dab_firmware_bin_start");
extern const std::uint8_t dab_firmware_bin_end[]
    asm("_binary_dab_firmware_bin_end");
extern const std::uint8_t fm_firmware_bin_start[]
    asm("_binary_fm_firmware_bin_start");
extern const std::uint8_t fm_firmware_bin_end[]
    asm("_binary_fm_firmware_bin_end");
}
/** @endcond */

namespace si4684 {

namespace {

const std::byte* asBytes(const std::uint8_t* ptr)
{
    return reinterpret_cast<const std::byte*>(ptr);
}

std::size_t embeddedSize(const std::uint8_t* start, const std::uint8_t* end)
{
    return static_cast<std::size_t>(end - start);
}

} // namespace

Si4684EmbeddedImages::Si4684EmbeddedImages()
    : patch_(asBytes(rom_patch_016_bin_start),
             embeddedSize(rom_patch_016_bin_start, rom_patch_016_bin_end))
    , dab_(asBytes(dab_firmware_bin_start),
           embeddedSize(dab_firmware_bin_start, dab_firmware_bin_end))
    , fm_(asBytes(fm_firmware_bin_start),
          embeddedSize(fm_firmware_bin_start, fm_firmware_bin_end))
{
}

const core::IFirmwareBlobReader& Si4684EmbeddedImages::romPatch() const noexcept
{
    return patch_;
}

const core::IFirmwareBlobReader& Si4684EmbeddedImages::dabFirmware() const noexcept
{
    return dab_;
}

const core::IFirmwareBlobReader& Si4684EmbeddedImages::fmFirmware() const noexcept
{
    return fm_;
}

const core::IFirmwareBlobReader& Si4684EmbeddedImages::applicationImage(
    Si4684Band band) const noexcept
{
    switch (band) {
    case Si4684Band::Dab:
        return dab_;
    case Si4684Band::Fm:
        return fm_;
    }
    return dab_;
}

} // namespace si4684
