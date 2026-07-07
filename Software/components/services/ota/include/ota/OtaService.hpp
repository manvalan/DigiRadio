/**
 * @file    OtaService.hpp
 * @brief   ESP32 firmware OTA streaming and rollback confirmation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include "core/OtaImageError.hpp"
#include "ota/OtaError.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace ota {

/**
 * @brief    OtaService — streams firmware images into the inactive OTA slot.
 *
 * @dname    OtaService
 * @return   n/a (type)
 * @pubstate Holds at most one active esp_ota session. Call confirmBoot()
 *           once after network bootstrap on every boot.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class OtaService {
public:
    /**
     * @brief    confirmBoot — cancel rollback after a healthy boot.
     *
     * @dname    confirmBoot
     * @return   Ok when pending verification is cleared or absent.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] static std::expected<void, OtaError> confirmBoot();

    /**
     * @brief    beginStream — open esp_ota session on the inactive slot.
     *
     * @dname    beginStream
     * @param    contentLength  Declared HTTP body size in bytes.
     * @return   Ok on success, or OtaError.
     * @pubstate starts an active session until finishStream() or abort().
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, OtaError> beginStream(int contentLength);

    /**
     * @brief    writeChunk — append bytes and validate the app descriptor once.
     *
     * @dname    writeChunk
     * @param    chunk  Next bytes from the HTTP body.
     * @return   Ok on success, core::OtaImageError on descriptor mismatch,
     *           or OtaError on flash write failure.
     * @pubstate accumulates header prefix until descriptor validation passes.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, OtaError> writeChunk(
        std::span<const std::uint8_t> chunk);

    /**
     * @brief    finishStream — finalize image and select the new boot slot.
     *
     * @dname    finishStream
     * @return   Ok when esp_ota_end and set_boot_partition succeed.
     * @pubstate closes the active session.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, OtaError> finishStream();

    /**
     * @brief    abort — cancel an in-progress OTA session.
     *
     * @dname    abort
     * @pubstate clears session state without changing the boot partition.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    void abort() noexcept;

    /**
     * @brief    lastImageError — image validation failure from writeChunk().
     *
     * @dname    lastImageError
     * @return   Most recent core::OtaImageError when writeChunk failed validation.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] core::OtaImageError lastImageError() const noexcept;

private:
    void resetSession() noexcept;

    bool active_ = false;
    bool descriptorValidated_ = false;
    int expectedSize_ = 0;
    int bytesWritten_ = 0;
    void* otaHandle_ = nullptr;
    const void* updatePartition_ = nullptr;
    std::vector<std::uint8_t> headerPrefix_;
    core::OtaImageError lastImageError_ = core::OtaImageError::InsufficientHeader;
};

} // namespace ota
