/**
 * @file    Si4684EmbeddedImages.hpp
 * @brief   Embedded Si4684 ROM patch, DAB and FM firmware blob accessors.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/EmbeddedBlobReader.hpp"
#include "si4684/Si4684Band.hpp"

namespace si4684 {

/**
 * @brief    Si4684EmbeddedImages — flash-backed Si4684 firmware blobs.
 *
 * @dname    Si4684EmbeddedImages
 * @return   n/a (type)
 * @pubstate Owns EmbeddedBlobReader views over EMBED_FILES symbols from
 *           Firmware/Si4684-Firmware/.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Si4684EmbeddedImages {
public:
    /**
     * @brief    Si4684EmbeddedImages — bind linker-embedded binaries.
     *
     * @dname    Si4684EmbeddedImages
     * @pubstate constructs patch_, dab_, and fm_ readers from flash symbols.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    Si4684EmbeddedImages();

    /**
     * @brief    romPatch — ROM patch blob for HOST_LOAD before main image.
     *
     * @dname    romPatch
     * @return   Reader over rom_patch_016.bin.
     * @pubstate reads patch_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const core::IFirmwareBlobReader& romPatch() const noexcept;

    /**
     * @brief    dabFirmware — DAB application image blob.
     *
     * @dname    dabFirmware
     * @return   Reader over dab_firmware.bin.
     * @pubstate reads dab_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const core::IFirmwareBlobReader& dabFirmware() const noexcept;

    /**
     * @brief    fmFirmware — FM application image blob.
     *
     * @dname    fmFirmware
     * @return   Reader over fm_firmware.bin.
     * @pubstate reads fm_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const core::IFirmwareBlobReader& fmFirmware() const noexcept;

    /**
     * @brief    applicationImage — map band to embedded application blob.
     *
     * @dname    applicationImage
     * @param    band  DAB or FM image selector.
     * @return   Reader for the selected application firmware.
     * @pubstate reads dab_ or fm_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const core::IFirmwareBlobReader& applicationImage(
        Si4684Band band) const noexcept;

private:
    core::EmbeddedBlobReader patch_;
    core::EmbeddedBlobReader dab_;
    core::EmbeddedBlobReader fm_;
};

} // namespace si4684
