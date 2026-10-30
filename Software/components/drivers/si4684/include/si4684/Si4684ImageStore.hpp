/**
 * @file    Si4684ImageStore.hpp
 * @brief   Si4684 firmware images kept in unpartitioned flash.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

#include "core/BlobStore.hpp"
#include "core/IFirmwareBlobReader.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esp_partition.h"

namespace si4684 {

/**
 * @brief    FlashBlobReader — one image inside the Si4684 image store.
 *
 * @dname    FlashBlobReader
 * @return   n/a (type)
 * @pubstate Reads nothing (size 0) until bound by Si4684ImageStore.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class FlashBlobReader final : public core::IFirmwareBlobReader {
public:
    /**
     * @brief    bind — point the reader at an image in the store.
     *
     * @dname    bind
     * @param    partition  Store partition.
     * @param    entry      Image placement from the store header.
     * @pubstate writes partition_ and entry_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void bind(const esp_partition_t* partition, core::BlobEntry entry) noexcept;

    /**
     * @brief    size — image length, 0 while unbound.
     *
     * @dname    size
     * @return   Bytes.
     * @pubstate reads entry_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::size_t size() const override;

    /**
     * @brief    read — copy image bytes from flash.
     *
     * @dname    read
     * @param    offset  From the start of the image.
     * @param    dest    Destination buffer.
     * @return   Bytes copied; 0 past the end, while unbound or on a flash error.
     * @pubstate reads flash through partition_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::size_t read(std::size_t offset,
                                   std::span<std::byte> dest) const override;

private:
    const esp_partition_t* partition_{nullptr};
    core::BlobEntry entry_{};
};

/**
 * @brief    Si4684ImageStore — Si4684 images in flash outside the app image.
 *
 * The images are proprietary and must not be published, so they cannot sit
 * in an app image released on GitHub. The store keeps them in flash the
 * partition table leaves unused (kFlashOffset, kCapacity), registered at
 * runtime as an external partition: no table change, no USB flash. A
 * firmware that still embeds the images fills the store once (fillFrom);
 * a firmware built without them reads them from here.
 *
 * @dname    Si4684ImageStore
 * @return   n/a (type)
 * @pubstate Owns three FlashBlobReader, bound by open().
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class Si4684ImageStore {
public:
    /** Start of the store: right after the "dsp" partition. */
    static constexpr std::uint32_t kFlashOffset = 0x860000U;
    /** 2 MB; the three images take about 1.05 MB. */
    static constexpr std::uint32_t kCapacity = 0x200000U;

    /** What GET /api/health reports. */
    enum class State : std::uint8_t {
        Unknown, ///< Not checked yet.
        Missing, ///< No valid store.
        Writing, ///< fillFrom() is copying the images.
        Ready,   ///< Header and every image CRC verified.
    };

    /**
     * @brief    open — register the flash region and verify the store.
     *
     * @dname    open
     * @return   true when header and all image CRCs check out.
     * @pubstate binds the readers on success; sets state_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] bool open();

    /**
     * @brief    fillFrom — copy images into the store unless already there.
     *
     * Erases and rewrites only when the stored header differs from the
     * sources (size or CRC). The header is written last, so an interrupted
     * copy leaves an invalid store that the next boot redoes.
     *
     * @dname    fillFrom
     * @param    patch  ROM patch.
     * @param    dab    DAB application image.
     * @param    fm     FM application image.
     * @return   true when the store ends up Ready.
     * @pubstate erases/writes flash at kFlashOffset; sets state_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] bool fillFrom(const core::IFirmwareBlobReader& patch,
                                const core::IFirmwareBlobReader& dab,
                                const core::IFirmwareBlobReader& fm);

    /**
     * @brief    state — store status for diagnostics.
     *
     * @dname    state
     * @return   Current State.
     * @pubstate reads state_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] State state() const noexcept;

    /**
     * @brief    stateToken — "ready", "missing", "writing" or "unknown".
     *
     * @dname    stateToken
     * @return   Static string.
     * @pubstate reads state_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] const char* stateToken() const noexcept;

    /** @brief romPatch — reader over the stored ROM patch. */
    [[nodiscard]] const core::IFirmwareBlobReader& romPatch() const noexcept;
    /** @brief dabFirmware — reader over the stored DAB image. */
    [[nodiscard]] const core::IFirmwareBlobReader& dabFirmware() const noexcept;
    /** @brief fmFirmware — reader over the stored FM image. */
    [[nodiscard]] const core::IFirmwareBlobReader& fmFirmware() const noexcept;

private:
    [[nodiscard]] const esp_partition_t* partition();

    const esp_partition_t* partition_{nullptr};
    std::array<FlashBlobReader, core::kBlobCount> readers_{};
    std::atomic<State> state_{State::Unknown};
};

} // namespace si4684
